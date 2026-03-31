#include "ProjectionRenderer.h"

#include "levishematic/schematic/placement/PlacementProjectionCache.h"
#include "levishematic/schematic/placement/PlacementStore.h"

#include "mc/client/renderer/chunks/RenderChunkCoordinator.h"

#include <unordered_set>

namespace levishematic::render {
namespace {

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
    placement::PlacementState const&         state,
    placement::PlacementProjectionCache&     placementCache
) {
    auto next = std::make_shared<ProjectionScene>();

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
        for (auto const& entry : projection.worldEntries) {
            auto posKey                 = util::encodePosKey(entry.pos);
            next->posColorMap[posKey]   = entry.color;
            next->bySubChunk[util::subChunkKeyFromWorldPos(entry.pos.x, entry.pos.y, entry.pos.z)].push_back(entry);
        }
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
    mPlacementCache->clear();
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

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (state.revision == mProjectedRevision) {
            return;
        }

        previousScene = mScene.load(std::memory_order_acquire);
        currentScene  = buildScene(state, *mPlacementCache);
        mScene.store(currentScene, std::memory_order_release);
        mProjectedRevision = state.revision;
    }

    if (triggerRefresh) {
        triggerRebuildForScene(coordinator, currentScene, previousScene);
    }
}

} // namespace levishematic::render
