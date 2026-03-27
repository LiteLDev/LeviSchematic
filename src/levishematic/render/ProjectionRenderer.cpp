#include "ProjectionRenderer.h"

#include "levishematic/schematic/placement/PlacementProjectionCache.h"
#include "levishematic/schematic/placement/PlacementStore.h"

#include "mc/client/renderer/chunks/RenderChunkCoordinator.h"

#include <algorithm>

namespace levishematic::render {
namespace {

void triggerRebuildForScene(
    std::shared_ptr<RenderChunkCoordinator> const& coordinator,
    std::shared_ptr<const ProjectionScene>         currentScene,
    std::shared_ptr<const ProjectionScene>         previousScene = nullptr,
    std::unordered_set<uint64_t> const*           dirtySubChunks = nullptr
) {
    if (!coordinator) {
        return;
    }

    std::unordered_set<uint64_t> dirtyKeys;
    auto markScene = [&](std::shared_ptr<const ProjectionScene> const& scene) {
        if (!scene) {
            return;
        }
        for (auto const& [subChunkKey, entries] : scene->bySubChunk) {
            if (!entries.empty()) {
                dirtyKeys.insert(subChunkKey);
            }
        }
    };

    if (dirtySubChunks) {
        dirtyKeys.insert(dirtySubChunks->begin(), dirtySubChunks->end());
    } else {
        markScene(previousScene);
        markScene(currentScene);
    }

    if (dirtyKeys.empty()) {
        return;
    }

    auto setDirtyForScene = [&](std::shared_ptr<const ProjectionScene> const& scene) {
        if (!scene) {
            return;
        }
        for (auto const& [subChunkKey, entries] : scene->bySubChunk) {
            if (dirtyKeys.erase(subChunkKey) == 0 || entries.empty()) {
                continue;
            }
            BlockPos const& pos = entries.front().pos;
            coordinator->_setDirty(pos, pos, false, false, false);
        }
    };

    setDirtyForScene(previousScene);
    setDirtyForScene(currentScene);

    for (auto const& subChunkKey : dirtyKeys) {
        int sx = static_cast<int>(subChunkKey >> 42);
        int sz = static_cast<int>((subChunkKey >> 21) & 0x1FFFFFu);
        int sy = static_cast<int>(subChunkKey & 0x1FFFFFu);

        if (sx & (1 << 21)) sx |= ~0x1FFFFF;
        if (sz & (1 << 20)) sz |= ~0x1FFFFF;
        if (sy & (1 << 20)) sy |= ~0x1FFFFF;

        BlockPos pos{sx << 4, sy << 4, sz << 4};
        coordinator->_setDirty(pos, pos, false, false, false);
    }
}

} // namespace

thread_local std::shared_ptr<const ProjectionScene> tl_currentScene;
thread_local bool                                   tl_hasProjection = false;

class ProjectionProjector::ProjectionIndex {
public:
    void clear() {
        mPlacements.clear();
        mContributorsByPos.clear();
        mResolvedByPos.clear();
        mResolvedBySubChunk.clear();
        mDirtySubChunks.clear();
        mDirtyPosKeys.clear();
        mNextLoadOrder = 1;
    }

    void upsertPlacement(
        PlacementProjectionId        placementId,
        std::vector<ProjEntry> const& entries,
        bool                         renderEnabled
    ) {
        std::unordered_set<uint64_t> touchedPosKeys;
        auto& state = mPlacements[placementId];
        if (state.loadOrder == 0) {
            state.loadOrder = mNextLoadOrder++;
        }

        for (auto const& [posKey, ignored] : state.entriesByPos) {
            (void)ignored;
            touchedPosKeys.insert(posKey);
            removeContributorFromPos(posKey, placementId);
        }

        state.entriesByPos.clear();
        state.renderEnabled = renderEnabled;
        for (auto const& entry : entries) {
            auto posKey = util::encodePosKey(entry.pos);
            state.entriesByPos[posKey] = entry;
            touchedPosKeys.insert(posKey);
            if (renderEnabled) {
                addContributorToPos(posKey, placementId);
            }
        }

        resolveDirtyPositions(touchedPosKeys);
    }

    void removePlacement(PlacementProjectionId placementId) {
        auto it = mPlacements.find(placementId);
        if (it == mPlacements.end()) {
            return;
        }

        std::unordered_set<uint64_t> touchedPosKeys;
        for (auto const& [posKey, ignored] : it->second.entriesByPos) {
            (void)ignored;
            touchedPosKeys.insert(posKey);
            removeContributorFromPos(posKey, placementId);
        }

        mPlacements.erase(it);
        resolveDirtyPositions(touchedPosKeys);
    }

    [[nodiscard]] std::shared_ptr<const ProjectionScene> publish(
        std::shared_ptr<const ProjectionScene> const& previousScene
    ) const {
        auto next = previousScene
            ? std::make_shared<ProjectionScene>(*previousScene)
            : std::make_shared<ProjectionScene>();

        for (auto posKey : mDirtyPosKeys) {
            if (auto it = mResolvedByPos.find(posKey); it != mResolvedByPos.end()) {
                next->posColorMap[posKey] = it->second.color;
            } else {
                next->posColorMap.erase(posKey);
            }
        }

        for (auto subChunkKey : mDirtySubChunks) {
            if (auto it = mResolvedBySubChunk.find(subChunkKey);
                it != mResolvedBySubChunk.end() && !it->second.empty()) {
                next->bySubChunk[subChunkKey] = it->second;
            } else {
                next->bySubChunk.erase(subChunkKey);
            }
        }

        return next;
    }

    [[nodiscard]] bool hasPendingChanges() const {
        return !mDirtySubChunks.empty() || !mDirtyPosKeys.empty();
    }

    [[nodiscard]] std::unordered_set<uint64_t> const& dirtySubChunks() const {
        return mDirtySubChunks;
    }

    void clearDirty() {
        mDirtySubChunks.clear();
        mDirtyPosKeys.clear();
    }

private:
    struct PlacementStateRecord {
        uint64_t                                loadOrder     = 0;
        bool                                    renderEnabled = true;
        std::unordered_map<uint64_t, ProjEntry> entriesByPos;
    };

    void resolveDirtyPositions(std::unordered_set<uint64_t> const& touchedPosKeys) {
        for (auto posKey : touchedPosKeys) {
            auto previous = [&]() -> std::optional<ProjEntry> {
                auto it = mResolvedByPos.find(posKey);
                return it == mResolvedByPos.end() ? std::nullopt : std::optional<ProjEntry>{it->second};
            }();

            auto next = resolveEntryForPos(posKey);
            updateResolvedEntry(posKey, previous, next);
        }
    }

    void updateResolvedEntry(
        uint64_t                 posKey,
        std::optional<ProjEntry> previousEntry,
        std::optional<ProjEntry> nextEntry
    ) {
        mDirtyPosKeys.insert(posKey);

        auto markSubChunk = [&](std::optional<ProjEntry> const& entry) {
            if (!entry) {
                return;
            }
            mDirtySubChunks.insert(util::subChunkKeyFromWorldPos(entry->pos.x, entry->pos.y, entry->pos.z));
        };
        markSubChunk(previousEntry);
        markSubChunk(nextEntry);

        if (previousEntry) {
            auto previousSubChunkKey = util::subChunkKeyFromWorldPos(
                previousEntry->pos.x,
                previousEntry->pos.y,
                previousEntry->pos.z
            );
            auto subChunkIt = mResolvedBySubChunk.find(previousSubChunkKey);
            if (subChunkIt != mResolvedBySubChunk.end()) {
                auto& bucket = subChunkIt->second;
                bucket.erase(
                    std::remove_if(bucket.begin(), bucket.end(), [&](ProjEntry const& entry) {
                        return util::encodePosKey(entry.pos) == posKey;
                    }),
                    bucket.end()
                );
                if (bucket.empty()) {
                    mResolvedBySubChunk.erase(subChunkIt);
                }
            }
            mResolvedByPos.erase(posKey);
        }

        if (nextEntry) {
            mResolvedByPos[posKey] = *nextEntry;
            auto nextSubChunkKey = util::subChunkKeyFromWorldPos(nextEntry->pos.x, nextEntry->pos.y, nextEntry->pos.z);
            auto& bucket = mResolvedBySubChunk[nextSubChunkKey];
            bucket.erase(
                std::remove_if(bucket.begin(), bucket.end(), [&](ProjEntry const& entry) {
                    return util::encodePosKey(entry.pos) == posKey;
                }),
                bucket.end()
            );
            bucket.push_back(*nextEntry);
        }
    }

    void removeContributorFromPos(uint64_t posKey, PlacementProjectionId placementId) {
        auto it = mContributorsByPos.find(posKey);
        if (it == mContributorsByPos.end()) {
            return;
        }

        auto& contributors = it->second;
        contributors.erase(
            std::remove(contributors.begin(), contributors.end(), placementId),
            contributors.end()
        );
        if (contributors.empty()) {
            mContributorsByPos.erase(it);
        }
    }

    void addContributorToPos(uint64_t posKey, PlacementProjectionId placementId) {
        auto& contributors = mContributorsByPos[posKey];
        if (std::find(contributors.begin(), contributors.end(), placementId) == contributors.end()) {
            contributors.push_back(placementId);
        }
    }

    [[nodiscard]] std::optional<ProjEntry> resolveEntryForPos(uint64_t posKey) const {
        auto contributorsIt = mContributorsByPos.find(posKey);
        if (contributorsIt == mContributorsByPos.end() || contributorsIt->second.empty()) {
            return std::nullopt;
        }

        PlacementProjectionId winnerId    = 0;
        uint64_t              winnerOrder = 0;
        for (auto placementId : contributorsIt->second) {
            auto placementIt = mPlacements.find(placementId);
            if (placementIt == mPlacements.end() || !placementIt->second.renderEnabled) {
                continue;
            }
            if (placementIt->second.loadOrder >= winnerOrder) {
                winnerOrder = placementIt->second.loadOrder;
                winnerId    = placementId;
            }
        }

        if (winnerId == 0) {
            return std::nullopt;
        }

        auto placementIt = mPlacements.find(winnerId);
        if (placementIt == mPlacements.end()) {
            return std::nullopt;
        }

        auto entryIt = placementIt->second.entriesByPos.find(posKey);
        return entryIt == placementIt->second.entriesByPos.end()
            ? std::nullopt
            : std::optional<ProjEntry>{entryIt->second};
    }

    uint64_t mNextLoadOrder = 1;
    std::unordered_map<PlacementProjectionId, PlacementStateRecord> mPlacements;
    std::unordered_map<uint64_t, std::vector<PlacementProjectionId>> mContributorsByPos;
    std::unordered_map<uint64_t, ProjEntry> mResolvedByPos;
    std::unordered_map<uint64_t, std::vector<ProjEntry>> mResolvedBySubChunk;
    std::unordered_set<uint64_t> mDirtySubChunks;
    std::unordered_set<uint64_t> mDirtyPosKeys;
};

ProjectionProjector::ProjectionProjector()
    : mScene(std::make_shared<const ProjectionScene>())
    , mIndex(std::make_unique<ProjectionIndex>())
    , mPlacementCache(std::make_unique<placement::PlacementProjectionCache>()) {}

ProjectionProjector::~ProjectionProjector() = default;

std::shared_ptr<const ProjectionScene> ProjectionProjector::scene() const {
    return mScene.load(std::memory_order_acquire);
}

bool ProjectionProjector::needsRefresh(uint64_t placementsRevision) const {
    std::lock_guard<std::mutex> lock(mMutex);
    return placementsRevision != mProjectedRevision;
}

void ProjectionProjector::rebuild(placement::PlacementState const& state) {
    rebuildLocked(state, nullptr, false);
}

void ProjectionProjector::rebuildAndRefresh(
    placement::PlacementState const&               state,
    std::shared_ptr<RenderChunkCoordinator> const& coordinator
) {
    rebuildLocked(state, coordinator, true);
}

void ProjectionProjector::triggerRebuild(std::shared_ptr<RenderChunkCoordinator> const& coordinator) const {
    triggerRebuildForScene(coordinator, scene());
}

void ProjectionProjector::clear() {
    std::lock_guard<std::mutex> lock(mMutex);
    mIndex->clear();
    mPlacementCache->clear();
    mKnownPlacementIds.clear();
    mAppliedRevisions.clear();
    mProjectedRevision = 0;
    mScene.store(std::make_shared<const ProjectionScene>(), std::memory_order_release);
}

void ProjectionProjector::rebuildLocked(
    placement::PlacementState const&               state,
    std::shared_ptr<RenderChunkCoordinator> const& coordinator,
    bool                                           triggerRefresh
) {
    std::shared_ptr<const ProjectionScene> previousScene;
    std::shared_ptr<const ProjectionScene> currentScene;
    std::unordered_set<uint64_t> dirtySubChunks;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (state.revision == mProjectedRevision) {
            return;
        }

        previousScene = mScene.load(std::memory_order_acquire);

        std::unordered_set<PlacementProjectionId> seenPlacementIds;
        for (auto placementId : state.order) {
            auto placementIt = state.placements.find(placementId);
            if (placementIt == state.placements.end()) {
                continue;
            }

            auto const& placement = placementIt->second;
            seenPlacementIds.insert(placement.id);

            auto revisionIt = mAppliedRevisions.find(placement.id);
            if (revisionIt != mAppliedRevisions.end() && revisionIt->second == placement.revision) {
                continue;
            }

            auto projection = mPlacementCache->view(placement);
            mIndex->upsertPlacement(
                placement.id,
                projection.worldEntries,
                placement.enabled && placement.renderEnabled
            );
            mAppliedRevisions[placement.id] = placement.revision;
            mKnownPlacementIds.insert(placement.id);
        }

        for (auto it = mKnownPlacementIds.begin(); it != mKnownPlacementIds.end();) {
            if (seenPlacementIds.contains(*it)) {
                ++it;
                continue;
            }
            mIndex->removePlacement(*it);
            mPlacementCache->remove(*it);
            mAppliedRevisions.erase(*it);
            it = mKnownPlacementIds.erase(it);
        }

        if (!mIndex->hasPendingChanges()) {
            mProjectedRevision = state.revision;
            return;
        }

        dirtySubChunks = mIndex->dirtySubChunks();
        currentScene   = mIndex->publish(previousScene);
        mScene.store(currentScene, std::memory_order_release);
        mIndex->clearDirty();
        mProjectedRevision = state.revision;
    }

    if (triggerRefresh) {
        triggerRebuildForScene(
            coordinator,
            std::move(currentScene),
            std::move(previousScene),
            dirtySubChunks.empty() ? nullptr : &dirtySubChunks
        );
    }
}

} // namespace levishematic::render
