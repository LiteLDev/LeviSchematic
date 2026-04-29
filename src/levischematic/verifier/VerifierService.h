#pragma once

#include "levischematic/editor/EditorState.h"
#include "levischematic/render/ProjectionRenderer.h"
#include "levischematic/schematic/placement/PlacementProjectionCache.h"
#include "levischematic/schematic/placement/PlacementStore.h"
#include "levischematic/verifier/VerifierTypes.h"

#include "ll/api/event/ListenerBase.h"

#include <memory>
#include <unordered_map>

class RenderChunkCoordinator;
class BlockSource;
class Block;

namespace levischematic::verifier_block_listener {
class VerifierBlockListener;
}

namespace levischematic::verifier {

class VerifierService {
public:
    VerifierService(
        VerifierState&                   state,
        placement::PlacementState const& placementState,
        editor::ViewState const&         viewState,
        render::ProjectionProjector&     projector
    );
    ~VerifierService();

    void handleBlockChanged(BlockSource& source, BlockPos const& pos, Block const& block);
    void refresh();
    void refresh(BlockSource& source);
    void handleJoinLevel();
    void handleExitLevel();
    void handleDimensionChanged();
    void ensureRuntimeBindings();
    void clear();
    void attachToRuntime();
    void detachFromRuntime();

    [[nodiscard]] VerifierState const& state() const { return mState; }

private:
    [[nodiscard]] VerificationStatus evaluateBlock(
        ExpectedBlockSnapshot const& expected,
        BlockSource&                 source,
        Block const&                 block
    ) const;
    void syncExpectedBlocks();
    void clearStatuses();
    void updateStatus(int dimensionId, BlockPos const& pos, VerificationStatus status);
    [[nodiscard]] std::shared_ptr<RenderChunkCoordinator> resolveCoordinator(BlockSource const& source) const;
    [[nodiscard]] BlockSource* resolveCurrentBlockSource() const;

    VerifierState&                   mState;
    placement::PlacementState const& mPlacementState;
    editor::ViewState const&         mViewState;
    render::ProjectionProjector&     mProjector;
    std::unique_ptr<levischematic::verifier_block_listener::VerifierBlockListener> mListener;
    std::unique_ptr<placement::PlacementProjectionCache>                         mPlacementCache;
    std::unordered_map<util::WorldBlockKey, ExpectedBlockSnapshot, util::WorldBlockKeyHash> mExpectedBlocksByKey;
    std::unordered_map<int, BlockSource*>                                        mSourcesByDimension;
    uint64_t                                                                     mExpectedPlacementsRevision = 0;
    uint64_t                                                                     mExpectedViewRevision       = 0;
};

} // namespace levischematic::verifier
