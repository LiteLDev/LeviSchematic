#pragma once
#include "VerificationPollSchedule.h"
#include "levischematic/editor/EditorState.h"
#include "levischematic/util/WorkBudget.h"
#include <deque>
#include <unordered_set>
namespace levischematic::verifier_block_listener {
class VerifierBlockListener;
}
class BlockSource;
class ClientInstance;
namespace levischematic::verifier {

class VerifierService {
public:
    
    VerifierService(VerifierState&, placement::PlacementState const&, editor::ViewState const&);
    
    ~VerifierService();
    
    void handleBlockChanged(BlockSource&, BlockPos const&, Block const&);
    
    void refresh();
    
    void refresh(BlockSource&);
    
    void tick(util::WorkBudget const& budget);
    
    void handleJoinLevel(ClientInstance& client);
    
    void handleExitLevel();
    
    void ensureRuntimeBindings(ClientInstance& client);
    
    void clear();
    
    void attachToRuntime(ClientInstance& client);
    
    void detachFromRuntime();
    
    void handleSourceDestroyed(BlockSource&);
    
    bool ownsListener(verifier_block_listener::VerifierBlockListener const& listener) const {
        return mListener.get() == &listener;
    }
    
    VerifierState const& state() const { return mState; }

private:
    
    bool evaluate(int dimension, BlockPos const& pos, VerificationCell& cell);
    VerificationStatus compare(Block const& expected, Block const& actual);
    
    void insert(placement::PlacementInstance const&, BlockPos, Block const*);
    
    void                                                             update(util::WorldBlockKey);
    VerifierState&                                                   mState;
    placement::PlacementState const&                                 mPlacements;
    std::shared_ptr<verifier_block_listener::VerifierBlockListener>  mListener;
    std::unordered_map<int, BlockSource*>                            mSources;
    std::unordered_map<Block const*, BlockCompareSpec>               mSpecs;
    std::deque<util::WorldBlockKey>                                  mKeys;
    std::deque<util::WorldBlockKey>                                  mDirty;
    std::unordered_set<util::WorldBlockKey, util::WorldBlockKeyHash> mDirtySet;
    uint64_t                                                         mRevision = 0;
    VerificationPollSchedule mPolling;
    size_t mBuildPlacement = 0, mBlockCursor = 0;
    bool   mBuilding = false, mRefreshRequested = false, mOverrideStarted = false;
    std::unordered_map<uint64_t, placement::OverrideEntry>::const_iterator mOverride;
};
} 
