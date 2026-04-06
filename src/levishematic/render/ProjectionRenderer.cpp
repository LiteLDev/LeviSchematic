#include "ProjectionRenderer.h"

#include "levishematic/schematic/placement/PlacementProjectionCache.h"
#include "levishematic/schematic/placement/PlacementStore.h"

#include "mc/client/renderer/chunks/RenderChunkCoordinator.h"

#include <unordered_set>

namespace levishematic::render {
namespace {

mce::Color colorForStatus(verifier::VerificationStatus status) {
    switch (status) {
    case verifier::VerificationStatus::Matched:
        return kDefaultProjectionColor;
    case verifier::VerificationStatus::PropertyMismatch:
        return kPropertyMismatchProjectionColor;
    case verifier::VerificationStatus::BlockMismatch:
        return kBlockMismatchProjectionColor;
    case verifier::VerificationStatus::Unknown:
    default:
        return kDefaultProjectionColor;
    }
}

void markSceneSubChunks(
    std::unordered_set<uint64_t>&              dirtyKeys,
    std::shared_ptr<const ProjectionScene> const& scene
) {
    if (!scene) {
        return;
    }

    for (auto const& [subChunkKey, entries] : scene->bySubChunk) {
        if (!entries.empty()) {
            dirtyKeys.insert(subChunkKey);
        }
    }
}

void triggerRebuildForScene(
    std::shared_ptr<RenderChunkCoordinator> const& coordinator,
    std::shared_ptr<const ProjectionScene> const&  currentScene,
    std::shared_ptr<const ProjectionScene> const&  previousScene = nullptr
) {
    if (!coordinator) {
        return;
    }

    std::unordered_set<uint64_t> dirtyKeys;
    markSceneSubChunks(dirtyKeys, previousScene);
    markSceneSubChunks(dirtyKeys, currentScene);

    for (auto const& subChunkKey : dirtyKeys) {
        if (currentScene) {
            auto currentIt = currentScene->bySubChunk.find(subChunkKey);
            if (currentIt != currentScene->bySubChunk.end() && !currentIt->second.empty()) {
                auto const& pos = currentIt->second.front().pos;
                coordinator->_setDirty(pos, pos, false, false, false);
                continue;
            }
        }

        if (previousScene) {
            auto previousIt = previousScene->bySubChunk.find(subChunkKey);
            if (previousIt != previousScene->bySubChunk.end() && !previousIt->second.empty()) {
                auto const& pos = previousIt->second.front().pos;
                coordinator->_setDirty(pos, pos, false, false, false);
            }
        }
    }
}

std::shared_ptr<const ProjectionScene> buildScene(
    placement::PlacementState const&     state,
    verifier::VerifierState const&       verifierState,
    placement::PlacementProjectionCache& placementCache
) {
    auto next = std::make_shared<ProjectionScene>();
    std::unordered_map<uint64_t, ProjEntry> entriesByPos;

    for (auto placementId : state.order) {
        auto placementIt = state.placements.find(placementId);
        if (placementIt == state.placements.end()) {
            continue;
        }

        auto const& placement = placementIt->second;
        if (!placement.enabled || !placement.renderEnabled) {
            continue;
        }

        auto projection = placementCache.view(placement);
        for (auto const& [posKey, expected] : projection.expectedBlocksByPos) {
            auto statusIt = verifierState.statusByPos.find(posKey);
            auto status = statusIt == verifierState.statusByPos.end()
                ? verifier::VerificationStatus::Unknown
                : statusIt->second;
            if (verifier::isHiddenStatus(status)) {
                entriesByPos.erase(posKey);
                next->posColorMap.erase(posKey);
                continue;
            }

            ProjEntry entry{
                .pos   = expected.pos,
                .block = expected.renderBlock,
                .color = colorForStatus(status),
            };
            entriesByPos[posKey]     = entry;
            next->posColorMap[posKey] = entry.color;
        }
    }

    for (auto const& [posKey, entry] : entriesByPos) {
        (void)posKey;
        next->bySubChunk[util::subChunkKeyFromWorldPos(entry.pos.x, entry.pos.y, entry.pos.z)].push_back(entry);
    }

    return next;
}

} // namespace

thread_local std::shared_ptr<const ProjectionScene> tl_currentScene;
thread_local bool                                   tl_hasProjection = false;

ProjectionProjector::ProjectionProjector()
    : mScene(std::make_shared<const ProjectionScene>())
    , mPlacementCache(std::make_unique<placement::PlacementProjectionCache>()) {}

ProjectionProjector::~ProjectionProjector() = default;

std::shared_ptr<const ProjectionScene> ProjectionProjector::scene() const {
    return mScene.load(std::memory_order_acquire);
}

bool ProjectionProjector::needsRefresh(uint64_t placementsRevision, uint64_t verifierRevision) const {
    std::lock_guard<std::mutex> lock(mMutex);
    return placementsRevision != mProjectedRevision || verifierRevision != mVerifierRevision;
}

void ProjectionProjector::rebuild(
    placement::PlacementState const& state,
    verifier::VerifierState const&   verifierState
) {
    rebuildLocked(state, verifierState, nullptr, false);
}

void ProjectionProjector::rebuildAndRefresh(
    placement::PlacementState const&               state,
    verifier::VerifierState const&                 verifierState,
    std::shared_ptr<RenderChunkCoordinator> const& coordinator
) {
    rebuildLocked(state, verifierState, coordinator, true);
}

void ProjectionProjector::triggerRebuild(std::shared_ptr<RenderChunkCoordinator> const& coordinator) const {
    triggerRebuildForScene(coordinator, scene());
}

void ProjectionProjector::triggerRebuildForPosition(
    BlockPos const&                                pos,
    std::shared_ptr<RenderChunkCoordinator> const& coordinator
) const {
    if (!coordinator) {
        return;
    }

    coordinator->_setDirty(pos, pos, false, false, false);
}

void ProjectionProjector::clear() {
    std::lock_guard<std::mutex> lock(mMutex);
    mPlacementCache->clear();
    mProjectedRevision = 0;
    mVerifierRevision  = 0;
    mScene.store(std::make_shared<const ProjectionScene>(), std::memory_order_release);
}

void ProjectionProjector::rebuildLocked(
    placement::PlacementState const&               state,
    verifier::VerifierState const&                 verifierState,
    std::shared_ptr<RenderChunkCoordinator> const& coordinator,
    bool                                           triggerRefresh
) {
    std::shared_ptr<const ProjectionScene> previousScene;
    std::shared_ptr<const ProjectionScene> currentScene;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (state.revision == mProjectedRevision && verifierState.revision == mVerifierRevision) {
            return;
        }

        previousScene = mScene.load(std::memory_order_acquire);
        currentScene  = buildScene(state, verifierState, *mPlacementCache);
        mScene.store(currentScene, std::memory_order_release);
        mProjectedRevision = state.revision;
        mVerifierRevision  = verifierState.revision;
    }

    if (triggerRefresh) {
        triggerRebuildForScene(coordinator, currentScene, previousScene);
    }
}

} // namespace levishematic::render
