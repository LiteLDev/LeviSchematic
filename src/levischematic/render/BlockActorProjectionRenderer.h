#pragma once

#include "levischematic/util/PositionUtils.h"
#include "levischematic/verifier/VerifierTypes.h"

#include "mc/deps/core/math/Color.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/actor/BlockActorRendererId.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class AABB;
class BlockActor;
class RenderChunkCoordinator;

namespace levischematic::placement {
class PlacementProjectionCache;
struct PlacementState;
} // namespace levischematic::placement

namespace levischematic::editor {
struct ViewState;
}

namespace levischematic::render {

struct BlockActorValidationOverlay {
    mce::Color color;
};

struct BlockActorProjEntry {
    BlockPos                                      pos;
    const Block*                                  block = nullptr;
    std::shared_ptr<BlockActor>                   blockActor;
    BlockActorRendererId                          rendererId = BlockActorRendererId::Default;
    mce::Color                                    color;
    std::optional<BlockActorValidationOverlay>    validationOverlay;
};

struct BlockActorProjectionScene {
    struct DimensionScene {
        std::unordered_map<uint64_t, std::vector<BlockActorProjEntry>> bySubChunk;

        [[nodiscard]] bool empty() const { return bySubChunk.empty(); }
    };

    std::unordered_map<int, DimensionScene> byDimension;

    [[nodiscard]] bool empty() const { return byDimension.empty(); }
};

[[nodiscard]] std::vector<BlockActorProjEntry const*>
collectBlockActorsInAabb(BlockActorProjectionScene::DimensionScene const& scene, AABB const& bounds);

class BlockActorProjectionProjector {
public:
    BlockActorProjectionProjector();
    ~BlockActorProjectionProjector();

    [[nodiscard]] std::shared_ptr<const BlockActorProjectionScene>                 scene() const;
    [[nodiscard]] std::shared_ptr<const BlockActorProjectionScene::DimensionScene> sceneForDimension(
        int dimensionId
    ) const;
    [[nodiscard]] bool
    needsRefresh(uint64_t placementsRevision, uint64_t verifierRevision, uint64_t viewRevision) const;

    void rebuild(
        placement::PlacementState const&        state,
        verifier::BlockActorVerifierState const& verifierState,
        editor::ViewState const&                viewState
    );
    void rebuildAndRefresh(
        placement::PlacementState const&                 state,
        verifier::BlockActorVerifierState const&         verifierState,
        editor::ViewState const&                         viewState,
        std::shared_ptr<RenderChunkCoordinator> const&   coordinator
    );
    void triggerRebuild(std::shared_ptr<RenderChunkCoordinator> const& coordinator) const;
    void triggerRebuildForPosition(
        int                                            dimensionId,
        BlockPos const&                                pos,
        std::shared_ptr<RenderChunkCoordinator> const& coordinator
    ) const;
    void clear();

private:
    void rebuildLocked(
        placement::PlacementState const&                 state,
        verifier::BlockActorVerifierState const&         verifierState,
        editor::ViewState const&                         viewState,
        std::shared_ptr<RenderChunkCoordinator> const&   coordinator,
        bool                                             triggerRefresh
    );

    std::atomic<std::shared_ptr<const BlockActorProjectionScene>> mScene;
    std::unique_ptr<placement::PlacementProjectionCache>          mPlacementCache;
    uint64_t                                                      mProjectedRevision = 0;
    uint64_t                                                      mVerifierRevision  = 0;
    uint64_t                                                      mViewRevision      = 0;
    mutable std::mutex                                            mMutex;
};

} // namespace levischematic::render
