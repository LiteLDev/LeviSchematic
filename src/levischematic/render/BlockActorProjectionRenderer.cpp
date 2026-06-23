#include "BlockActorProjectionRenderer.h"

#include "levischematic/editor/EditorState.h"
#include "levischematic/schematic/placement/PlacementProjectionCache.h"
#include "levischematic/schematic/placement/PlacementStore.h"

#include "mc/client/renderer/chunks/RenderChunkCoordinator.h"
#include "mc/world/phys/AABB.h"

#include <cmath>

namespace levischematic::render {
namespace {

void markSceneSubChunks(
    std::unordered_set<uint64_t>&                         dirtyKeys,
    BlockActorProjectionScene::DimensionScene const* scene
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
    std::shared_ptr<RenderChunkCoordinator> const&              coordinator,
    BlockActorProjectionScene::DimensionScene const* currentScene,
    BlockActorProjectionScene::DimensionScene const* previousScene = nullptr
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
                coordinator->_setDirty(pos, pos, true, false, false);
                continue;
            }
        }

        if (previousScene) {
            auto previousIt = previousScene->bySubChunk.find(subChunkKey);
            if (previousIt != previousScene->bySubChunk.end() && !previousIt->second.empty()) {
                auto const& pos = previousIt->second.front().pos;
                coordinator->_setDirty(pos, pos, true, false, false);
            }
        }
    }
}

int floorDiv16(int value) noexcept {
    return value / 16 - (value % 16 != 0 && value < 0 ? 1 : 0);
}

uint64_t encodeSubChunkCoordKey(int sx, int sy, int sz) noexcept {
    return util::encodeSubChunkKey(BlockPos{sx * 16, sy * 16, sz * 16});
}

bool intersectsBlock(AABB const& bounds, BlockPos const& pos) noexcept {
    return static_cast<float>(pos.x + 1) >= bounds.min.x && static_cast<float>(pos.x) <= bounds.max.x
        && static_cast<float>(pos.y + 1) >= bounds.min.y && static_cast<float>(pos.y) <= bounds.max.y
        && static_cast<float>(pos.z + 1) >= bounds.min.z && static_cast<float>(pos.z) <= bounds.max.z;
}

std::shared_ptr<const BlockActorProjectionScene> buildScene(
    placement::PlacementState const&             state,
    verifier::BlockActorVerifierState const&     verifierState,
    editor::ViewState const&                     viewState,
    placement::PlacementProjectionCache&         placementCache
) {
    auto next = std::make_shared<BlockActorProjectionScene>();

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
        auto& dimensionScene = next->byDimension[placement.dimensionId];

        for (auto entry : projection.blockActorEntries) {
            if (!entry.block || !entry.blockActor || !viewState.layerRange.contains(entry.pos.y)) {
                continue;
            }

            auto worldKey = util::makeWorldBlockKey(placement.dimensionId, entry.pos);
            auto statusIt = verifierState.statusByKey.find(worldKey);
            auto status = statusIt == verifierState.statusByKey.end()
                ? verifier::VerificationStatus::Unknown
                : statusIt->second;
            if (verifier::isHiddenStatus(status)) {
                continue;
            }

            auto subChunkKey = util::subChunkKeyFromWorldPos(entry.pos.x, entry.pos.y, entry.pos.z);
            dimensionScene.bySubChunk[subChunkKey].push_back(std::move(entry));
        }
    }

    return next;
}

} // namespace

std::vector<BlockActorProjEntry const*>
collectBlockActorsInAabb(BlockActorProjectionScene::DimensionScene const& scene, AABB const& bounds) {
    std::vector<BlockActorProjEntry const*> result;
    if (scene.bySubChunk.empty()) {
        return result;
    }

    auto minX = floorDiv16(static_cast<int>(std::floor(bounds.min.x)));
    auto minY = floorDiv16(static_cast<int>(std::floor(bounds.min.y)));
    auto minZ = floorDiv16(static_cast<int>(std::floor(bounds.min.z)));
    auto maxX = floorDiv16(static_cast<int>(std::floor(bounds.max.x)));
    auto maxY = floorDiv16(static_cast<int>(std::floor(bounds.max.y)));
    auto maxZ = floorDiv16(static_cast<int>(std::floor(bounds.max.z)));

    for (int sx = minX; sx <= maxX; ++sx) {
        for (int sy = minY; sy <= maxY; ++sy) {
            for (int sz = minZ; sz <= maxZ; ++sz) {
                auto it = scene.bySubChunk.find(encodeSubChunkCoordKey(sx, sy, sz));
                if (it == scene.bySubChunk.end()) {
                    continue;
                }

                for (auto const& entry : it->second) {
                    if (intersectsBlock(bounds, entry.pos)) {
                        result.push_back(&entry);
                    }
                }
            }
        }
    }

    return result;
}

BlockActorProjectionProjector::BlockActorProjectionProjector()
: mScene(std::make_shared<const BlockActorProjectionScene>()),
  mPlacementCache(std::make_unique<placement::PlacementProjectionCache>()) {}

BlockActorProjectionProjector::~BlockActorProjectionProjector() = default;

std::shared_ptr<const BlockActorProjectionScene> BlockActorProjectionProjector::scene() const {
    return mScene.load(std::memory_order_acquire);
}

std::shared_ptr<const BlockActorProjectionScene::DimensionScene>
BlockActorProjectionProjector::sceneForDimension(int dimensionId) const {
    auto current = scene();
    if (!current) {
        return nullptr;
    }

    auto it = current->byDimension.find(dimensionId);
    if (it == current->byDimension.end()) {
        return nullptr;
    }

    return std::shared_ptr<const BlockActorProjectionScene::DimensionScene>(current, &it->second);
}

bool BlockActorProjectionProjector::needsRefresh(
    uint64_t placementsRevision,
    uint64_t verifierRevision,
    uint64_t viewRevision
) const {
    std::lock_guard<std::mutex> lock(mMutex);
    return placementsRevision != mProjectedRevision || verifierRevision != mVerifierRevision
        || viewRevision != mViewRevision;
}

void BlockActorProjectionProjector::rebuild(
    placement::PlacementState const&          state,
    verifier::BlockActorVerifierState const& verifierState,
    editor::ViewState const&                  viewState
) {
    rebuildLocked(state, verifierState, viewState, nullptr, false);
}

void BlockActorProjectionProjector::rebuildAndRefresh(
    placement::PlacementState const&                state,
    verifier::BlockActorVerifierState const&        verifierState,
    editor::ViewState const&                        viewState,
    std::shared_ptr<RenderChunkCoordinator> const&  coordinator
) {
    rebuildLocked(state, verifierState, viewState, coordinator, true);
}

void BlockActorProjectionProjector::triggerRebuild(
    std::shared_ptr<RenderChunkCoordinator> const& coordinator
) const {
    auto current = scene();
    if (!current) {
        return;
    }

    for (auto const& [dimensionId, dimensionScene] : current->byDimension) {
        (void)dimensionId;
        triggerRebuildForScene(coordinator, &dimensionScene);
    }
}

void BlockActorProjectionProjector::triggerRebuildForPosition(
    int                                            dimensionId,
    BlockPos const&                                pos,
    std::shared_ptr<RenderChunkCoordinator> const& coordinator
) const {
    (void)dimensionId;
    if (!coordinator) {
        return;
    }

    coordinator->_setDirty(pos, pos, true, false, false);
}

void BlockActorProjectionProjector::clear() {
    std::lock_guard<std::mutex> lock(mMutex);
    mPlacementCache->clear();
    mProjectedRevision = 0;
    mVerifierRevision  = 0;
    mViewRevision      = 0;
    mScene.store(std::make_shared<const BlockActorProjectionScene>(), std::memory_order_release);
}

void BlockActorProjectionProjector::rebuildLocked(
    placement::PlacementState const&                state,
    verifier::BlockActorVerifierState const&        verifierState,
    editor::ViewState const&                        viewState,
    std::shared_ptr<RenderChunkCoordinator> const&  coordinator,
    bool                                            triggerRefresh
) {
    std::shared_ptr<const BlockActorProjectionScene> previousScene;
    std::shared_ptr<const BlockActorProjectionScene> currentScene;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (state.revision == mProjectedRevision && verifierState.revision == mVerifierRevision
            && viewState.revision == mViewRevision) {
            return;
        }

        previousScene = mScene.load(std::memory_order_acquire);
        currentScene  = buildScene(state, verifierState, viewState, *mPlacementCache);
        mScene.store(currentScene, std::memory_order_release);
        mProjectedRevision = state.revision;
        mVerifierRevision  = verifierState.revision;
        mViewRevision      = viewState.revision;
    }

    if (triggerRefresh) {
        std::unordered_set<int> dimensionIds;
        if (currentScene) {
            for (auto const& [dimensionId, scene] : currentScene->byDimension) {
                (void)scene;
                dimensionIds.insert(dimensionId);
            }
        }
        if (previousScene) {
            for (auto const& [dimensionId, scene] : previousScene->byDimension) {
                (void)scene;
                dimensionIds.insert(dimensionId);
            }
        }

        for (int dimensionId : dimensionIds) {
            BlockActorProjectionScene::DimensionScene const* currentDimensionScene  = nullptr;
            BlockActorProjectionScene::DimensionScene const* previousDimensionScene = nullptr;

            if (currentScene) {
                auto it = currentScene->byDimension.find(dimensionId);
                if (it != currentScene->byDimension.end()) {
                    currentDimensionScene = &it->second;
                }
            }

            if (previousScene) {
                auto it = previousScene->byDimension.find(dimensionId);
                if (it != previousScene->byDimension.end()) {
                    previousDimensionScene = &it->second;
                }
            }

            triggerRebuildForScene(coordinator, currentDimensionScene, previousDimensionScene);
        }
    }
}

} // namespace levischematic::render
