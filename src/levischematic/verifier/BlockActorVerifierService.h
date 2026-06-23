#pragma once

#include "levischematic/editor/EditorState.h"
#include "levischematic/render/BlockActorProjectionRenderer.h"
#include "levischematic/schematic/placement/PlacementProjectionCache.h"
#include "levischematic/schematic/placement/PlacementStore.h"
#include "levischematic/verifier/VerifierTypes.h"

#include <memory>
#include <unordered_map>

class Block;
class BlockSource;
class RenderChunkCoordinator;

namespace levischematic::verifier {

class BlockActorVerifierService {
public:
    BlockActorVerifierService(
        BlockActorVerifierState&           state,
        placement::PlacementState const&   placementState,
        editor::ViewState const&           viewState,
        render::BlockActorProjectionProjector& projector
    );
    ~BlockActorVerifierService();

    void handleBlockChanged(BlockSource& source, BlockPos const& pos, Block const& block);
    void refresh();
    void refresh(BlockSource& source);
    void clear();

    [[nodiscard]] BlockActorVerifierState const& state() const { return mState; }

private:
    [[nodiscard]] VerificationStatus evaluateBlockActor(
        BlockActorExpectedSnapshot const& expected,
        BlockSource&                      source,
        Block const&                      block
    ) const;
    void syncExpectedBlockActors();
    void clearStatuses();
    void updateStatus(int dimensionId, BlockPos const& pos, VerificationStatus status);
    [[nodiscard]] std::shared_ptr<RenderChunkCoordinator> resolveCoordinator(BlockSource const& source) const;

    BlockActorVerifierState& mState;
    placement::PlacementState const& mPlacementState;
    editor::ViewState const& mViewState;
    render::BlockActorProjectionProjector& mProjector;
    std::unique_ptr<placement::PlacementProjectionCache> mPlacementCache;
    std::unordered_map<util::WorldBlockKey, BlockActorExpectedSnapshot, util::WorldBlockKeyHash>
        mExpectedBlockActorsByKey;
    uint64_t mExpectedPlacementsRevision = 0;
    uint64_t mExpectedViewRevision       = 0;
};

} // namespace levischematic::verifier
