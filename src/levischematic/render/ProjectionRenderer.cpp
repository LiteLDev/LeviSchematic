#include "ProjectionRenderer.h"

#include "levischematic/LeviSchematic.h"
#include "levischematic/editor/EditorState.h"
#include "levischematic/schematic/placement/PlacementProjectionCache.h"
#include "levischematic/schematic/placement/PlacementStore.h"

#include "mc/client/renderer/chunks/RenderChunkCoordinator.h"
#include "mc/world/phys/AABB.h"

#include <cmath>
#include <unordered_set>

namespace levischematic::render {
namespace {

auto& getLogger() { return levischematic::LeviSchematic::getInstance().getSelf().getLogger(); }

void markSceneSubChunks(std::unordered_set<uint64_t>& dirtyKeys, ProjectionScene::DimensionScene const* scene) {
    if (!scene) {
        return;
    }

    for (auto const& [subChunkKey, entries] : scene->bySubChunk) {
        if (!entries.empty()) {
            dirtyKeys.insert(subChunkKey);
        }
    }

    for (auto const& [subChunkKey, entries] : scene->blockActorsBySubChunk) {
        if (!entries.empty()) {
            dirtyKeys.insert(subChunkKey);
        }
    }

    for (auto const& subChunkKey : scene->subChunksWithColorOverrides) {
        dirtyKeys.insert(subChunkKey);
    }
}

void triggerRebuildForScene(
    std::shared_ptr<RenderChunkCoordinator> const& coordinator,
    ProjectionScene::DimensionScene const*         currentScene,
    ProjectionScene::DimensionScene const*         previousScene = nullptr
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

            if (currentScene->subChunksWithColorOverrides.contains(subChunkKey)) {
                for (auto const& [posKey, color] : currentScene->posColorMap) {
                    (void)color;
                    auto const pos = util::decodePosKey(posKey);
                    if (util::subChunkKeyFromWorldPos(pos.x, pos.y, pos.z) == subChunkKey) {
                        coordinator->_setDirty(pos, pos, true, false, false);
                        break;
                    }
                }
                continue;
            }

            auto currentBlockActorIt = currentScene->blockActorsBySubChunk.find(subChunkKey);
            if (currentBlockActorIt != currentScene->blockActorsBySubChunk.end()
                && !currentBlockActorIt->second.empty()) {
                auto const& pos = currentBlockActorIt->second.front().pos;
                coordinator->_setDirty(pos, pos, true, false, false);
                continue;
            }
        }

        if (previousScene) {
            auto previousIt = previousScene->bySubChunk.find(subChunkKey);
            if (previousIt != previousScene->bySubChunk.end() && !previousIt->second.empty()) {
                auto const& pos = previousIt->second.front().pos;
                coordinator->_setDirty(pos, pos, true, false, false);
                continue;
            }

            if (previousScene->subChunksWithColorOverrides.contains(subChunkKey)) {
                for (auto const& [posKey, color] : previousScene->posColorMap) {
                    (void)color;
                    auto const pos = util::decodePosKey(posKey);
                    if (util::subChunkKeyFromWorldPos(pos.x, pos.y, pos.z) == subChunkKey) {
                        coordinator->_setDirty(pos, pos, true, false, false);
                        break;
                    }
                }
                continue;
            }

            auto previousBlockActorIt = previousScene->blockActorsBySubChunk.find(subChunkKey);
            if (previousBlockActorIt != previousScene->blockActorsBySubChunk.end()
                && !previousBlockActorIt->second.empty()) {
                auto const& pos = previousBlockActorIt->second.front().pos;
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

std::shared_ptr<const ProjectionScene> buildScene(
    placement::PlacementState const&     state,
    verifier::VerifierState const&       verifierState,
    editor::ViewState const&             viewState,
    placement::PlacementProjectionCache& placementCache,
    ProjectionColorResolver const&       colorResolver
) {
    auto                                                             next = std::make_shared<ProjectionScene>();
    std::unordered_map<int, std::unordered_map<uint64_t, ProjEntry>> entriesByDimension;

    for (auto placementId : state.order) {
        auto placementIt = state.placements.find(placementId);
        if (placementIt == state.placements.end()) {
            continue;
        }

        auto const& placement = placementIt->second;
        if (!placement.enabled || !placement.renderEnabled) {
            continue;
        }

        auto                         projection   = placementCache.view(placement);
        auto&                        entriesByPos = entriesByDimension[placement.dimensionId];
        std::unordered_set<uint64_t> blockActorPosKeys;
        blockActorPosKeys.reserve(projection.blockActorEntries.size());
        for (auto const& entry : projection.blockActorEntries) {
            blockActorPosKeys.insert(util::encodePosKey(entry.pos));
        }

        for (auto const& [worldKey, expected] : projection.expectedBlocksByKey) {
            if (!viewState.layerRange.contains(expected.pos.y)) {
                continue;
            }

            auto statusIt = verifierState.statusByKey.find(worldKey);
            auto status =
                statusIt == verifierState.statusByKey.end() ? verifier::VerificationStatus::Unknown : statusIt->second;
            auto& dimensionScene = next->byDimension[placement.dimensionId];
            auto  color          = colorResolver.resolveColor(*expected.renderBlock, status);
            auto  subChunkKey    = util::subChunkKeyFromWorldPos(expected.pos.x, expected.pos.y, expected.pos.z);
            if (verifier::isHiddenStatus(status)) {
                entriesByPos.erase(worldKey.posKey);
                dimensionScene.posColorMap.erase(worldKey.posKey);
                dimensionScene.subChunksWithColorOverrides.erase(subChunkKey);
                for (auto const& [otherPosKey, otherColor] : dimensionScene.posColorMap) {
                    (void)otherColor;
                    auto const otherPos = util::decodePosKey(otherPosKey);
                    if (util::subChunkKeyFromWorldPos(otherPos.x, otherPos.y, otherPos.z) == subChunkKey) {
                        dimensionScene.subChunksWithColorOverrides.insert(subChunkKey);
                        break;
                    }
                }
                continue;
            }

            if (status == verifier::VerificationStatus::PropertyMismatch) {
                entriesByPos.erase(worldKey.posKey);
                dimensionScene.posColorMap[worldKey.posKey] = color;
                dimensionScene.subChunksWithColorOverrides.insert(subChunkKey);
                continue;
            }
            if (status == verifier::VerificationStatus::BlockMismatch) {
                entriesByPos.erase(worldKey.posKey);
                dimensionScene.posColorMap[worldKey.posKey] = color;
                dimensionScene.subChunksWithColorOverrides.insert(subChunkKey);
                continue;
            }

            ProjEntry entry{
                .pos   = expected.pos,
                .block = expected.renderBlock,
                .color = color,
            };
            entriesByPos[worldKey.posKey]               = entry;
            dimensionScene.posColorMap[worldKey.posKey] = entry.color;
            dimensionScene.subChunksWithColorOverrides.insert(subChunkKey);
        }
        auto& dimensionScene = next->byDimension[placement.dimensionId];
        for (auto entry : projection.blockActorEntries) {
            if (!entry.block || !entry.blockActor) {
    getLogger().error(
        "buildScene invalid blockActor entry: dim={}, pos=({}, {}, {}), block={}, actor={}",
        placement.dimensionId,
        entry.pos.x,
        entry.pos.y,
        entry.pos.z,
        static_cast<bool>(entry.block),
        static_cast<bool>(entry.blockActor)
    );
    continue;
}

auto expectedIt = projection.expectedBlocksByKey.find(
    util::makeWorldBlockKey(placement.dimensionId, entry.pos)
);
if (expectedIt == projection.expectedBlocksByKey.end()) {
    getLogger().error(
        "buildScene blockActor has no expected block: dim={}, pos=({}, {}, {})",
        placement.dimensionId,
        entry.pos.x,
        entry.pos.y,
        entry.pos.z
    );
    continue;
}

if (expectedIt->second.renderBlock != entry.block) {
    getLogger().error(
        "buildScene blockActor/renderBlock mismatch: dim={}, pos=({}, {}, {})",
        placement.dimensionId,
        entry.pos.x,
        entry.pos.y,
        entry.pos.z
    );
}
            if (!entry.block || !entry.blockActor || !viewState.layerRange.contains(entry.pos.y)) {
                continue;
            }

            auto worldKey = util::makeWorldBlockKey(placement.dimensionId, entry.pos);
            auto statusIt = verifierState.statusByKey.find(worldKey);
            auto status =
                statusIt == verifierState.statusByKey.end() ? verifier::VerificationStatus::Unknown : statusIt->second;
            if (verifier::isHiddenStatus(status) || status == verifier::VerificationStatus::PropertyMismatch
                || status == verifier::VerificationStatus::BlockMismatch) {
                continue;
            }

            entry.color                                 = colorResolver.resolveColor(*entry.block, status);
            dimensionScene.posColorMap[worldKey.posKey] = entry.color;
            dimensionScene.subChunksWithColorOverrides.insert(
                util::subChunkKeyFromWorldPos(entry.pos.x, entry.pos.y, entry.pos.z)
            );
            auto blockActorSubChunkKey = util::subChunkKeyFromWorldPos(entry.pos.x, entry.pos.y, entry.pos.z);
            dimensionScene.blockActorsBySubChunk[blockActorSubChunkKey].push_back(std::move(entry));
        }
    }

    for (auto& [dimensionId, entriesByPos] : entriesByDimension) {
        auto& dimensionScene = next->byDimension[dimensionId];
        for (auto const& [posKey, entry] : entriesByPos) {
            (void)posKey;
            dimensionScene.bySubChunk[util::subChunkKeyFromWorldPos(entry.pos.x, entry.pos.y, entry.pos.z)].push_back(
                entry
            );
        }
    }

    return next;
}

} // namespace

thread_local std::shared_ptr<const ProjectionScene::DimensionScene> tl_currentScene;
thread_local bool                                                   tl_hasProjection = false;

std::vector<BlockActorProjEntry const*>
collectBlockActorsInAabb(ProjectionScene::DimensionScene const& scene, AABB const& bounds) {
    std::vector<BlockActorProjEntry const*> result;
    if (scene.blockActorsBySubChunk.empty()) {
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
                auto it = scene.blockActorsBySubChunk.find(encodeSubChunkCoordKey(sx, sy, sz));
                if (it == scene.blockActorsBySubChunk.end()) {
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

ProjectionProjector::ProjectionProjector()
: mScene(std::make_shared<const ProjectionScene>()),
  mPlacementCache(std::make_unique<placement::PlacementProjectionCache>()) {}

ProjectionProjector::~ProjectionProjector() = default;

std::shared_ptr<const ProjectionScene> ProjectionProjector::scene() const {
    return mScene.load(std::memory_order_acquire);
}

std::shared_ptr<const ProjectionScene::DimensionScene> ProjectionProjector::sceneForDimension(int dimensionId) const {
    auto current = scene();
    if (!current) {
        return nullptr;
    }

    auto it = current->byDimension.find(dimensionId);
    if (it == current->byDimension.end()) {
        return nullptr;
    }

    return std::shared_ptr<const ProjectionScene::DimensionScene>(current, &it->second);
}

bool ProjectionProjector::needsRefresh(uint64_t placementsRevision, uint64_t verifierRevision, uint64_t viewRevision)
    const {
    std::lock_guard<std::mutex> lock(mMutex);
    return placementsRevision != mProjectedRevision || verifierRevision != mVerifierRevision
        || viewRevision != mViewRevision;
}

void ProjectionProjector::rebuild(
    placement::PlacementState const& state,
    verifier::VerifierState const&   verifierState,
    editor::ViewState const&         viewState
) {
    rebuildLocked(state, verifierState, viewState, nullptr, false);
}

void ProjectionProjector::rebuildAndRefresh(
    placement::PlacementState const&               state,
    verifier::VerifierState const&                 verifierState,
    editor::ViewState const&                       viewState,
    std::shared_ptr<RenderChunkCoordinator> const& coordinator
) {
    rebuildLocked(state, verifierState, viewState, coordinator, true);
}

void ProjectionProjector::triggerRebuild(std::shared_ptr<RenderChunkCoordinator> const& coordinator) const {
    auto current = scene();
    if (!current) {
        return;
    }

    for (auto const& [dimensionId, dimensionScene] : current->byDimension) {
        (void)dimensionId;
        triggerRebuildForScene(coordinator, &dimensionScene);
    }
}

void ProjectionProjector::triggerRebuildForPosition(
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

void ProjectionProjector::clear() {
    std::lock_guard<std::mutex> lock(mMutex);
    mPlacementCache->clear();
    mProjectedRevision = 0;
    mVerifierRevision  = 0;
    mViewRevision      = 0;
    mScene.store(std::make_shared<const ProjectionScene>(), std::memory_order_release);
}

void ProjectionProjector::rebuildLocked(
    placement::PlacementState const&               state,
    verifier::VerifierState const&                 verifierState,
    editor::ViewState const&                       viewState,
    std::shared_ptr<RenderChunkCoordinator> const& coordinator,
    bool                                           triggerRefresh
) {
    std::shared_ptr<const ProjectionScene> previousScene;
    std::shared_ptr<const ProjectionScene> currentScene;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (state.revision == mProjectedRevision && verifierState.revision == mVerifierRevision
            && viewState.revision == mViewRevision) {
            return;
        }

        previousScene = mScene.load(std::memory_order_acquire);
        currentScene  = buildScene(state, verifierState, viewState, *mPlacementCache, mColorResolver);
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
            ProjectionScene::DimensionScene const* currentDimensionScene  = nullptr;
            ProjectionScene::DimensionScene const* previousDimensionScene = nullptr;

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
