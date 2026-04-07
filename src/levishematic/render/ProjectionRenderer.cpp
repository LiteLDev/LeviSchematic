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
    case verifier::VerificationStatus::MissingBlock:
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
    std::unordered_set<uint64_t>&          dirtyKeys,
    ProjectionScene::DimensionScene const* scene
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

std::shared_ptr<const ProjectionScene> buildScene(
    placement::PlacementState const&     state,
    verifier::VerifierState const&       verifierState,
    placement::PlacementProjectionCache& placementCache
) {
    auto next = std::make_shared<ProjectionScene>();
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

        auto projection      = placementCache.view(placement);
        auto& entriesByPos   = entriesByDimension[placement.dimensionId];
        auto& dimensionScene = next->byDimension[placement.dimensionId];
        for (auto const& [worldKey, expected] : projection.expectedBlocksByKey) {
            auto statusIt = verifierState.statusByKey.find(worldKey);
            auto status = statusIt == verifierState.statusByKey.end()
                ? verifier::VerificationStatus::Unknown
                : statusIt->second;
            if (verifier::isHiddenStatus(status)) {
                entriesByPos.erase(worldKey.posKey);
                dimensionScene.posColorMap.erase(worldKey.posKey);
                continue;
            }

            ProjEntry entry{
                .pos   = expected.pos,
                .block = expected.renderBlock,
                .color = colorForStatus(status),
            };
            entriesByPos[worldKey.posKey]               = entry;
            dimensionScene.posColorMap[worldKey.posKey] = entry.color;
        }
    }

    for (auto& [dimensionId, entriesByPos] : entriesByDimension) {
        auto& dimensionScene = next->byDimension[dimensionId];
        for (auto const& [posKey, entry] : entriesByPos) {
            (void)posKey;
            dimensionScene.bySubChunk[util::subChunkKeyFromWorldPos(entry.pos.x, entry.pos.y, entry.pos.z)]
                .push_back(entry);
        }
    }

    return next;
}

} // namespace

thread_local std::shared_ptr<const ProjectionScene::DimensionScene> tl_currentScene;
thread_local bool                                                   tl_hasProjection = false;

ProjectionProjector::ProjectionProjector()
    : mScene(std::make_shared<const ProjectionScene>())
    , mPlacementCache(std::make_unique<placement::PlacementProjectionCache>()) {}

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
    int                                             dimensionId,
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

} // namespace levishematic::render
