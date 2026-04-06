#pragma once

#include "levishematic/render/ProjectionRenderer.h"
#include "levishematic/schematic/placement/PlacementProjectionCache.h"
#include "levishematic/schematic/placement/PlacementStore.h"
#include "levishematic/verifier/VerifierTypes.h"

#include <memory>
#include <unordered_map>

class RenderChunkCoordinator;
class BlockSource;
class Block;

namespace levishematic::verifier_block_listener {
class VerifierBlockListener;
}

namespace levishematic::verifier {

class VerifierService {
public:
    VerifierService(
        VerifierState&                   state,
        placement::PlacementState const& placementState,
        render::ProjectionProjector&     projector
    );
    ~VerifierService();

    void handleBlockChanged(BlockSource& source, BlockPos const& pos, Block const& block);
    void refresh();
    void refresh(BlockSource& source);
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
    void updateStatus(BlockPos const& pos, VerificationStatus status);
    [[nodiscard]] std::shared_ptr<RenderChunkCoordinator> resolveCoordinator(BlockSource const& source) const;

    VerifierState&                   mState;
    placement::PlacementState const& mPlacementState;
    render::ProjectionProjector&     mProjector;
    std::unique_ptr<levishematic::verifier_block_listener::VerifierBlockListener> mListener;
    std::unique_ptr<placement::PlacementProjectionCache>                         mPlacementCache;
    std::unordered_map<uint64_t, ExpectedBlockSnapshot>                          mExpectedBlocksByPos;
    std::unordered_map<int, BlockSource*>                                        mSourcesByDimension;
    uint64_t                                                                     mExpectedPlacementsRevision = 0;
};

} // namespace levishematic::verifier
