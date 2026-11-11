#pragma once
#include "VerificationPollSchedule.h"
#include "VerificationTask.h"
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
    bool startTask(uint32_t placementId, bool layerOnly);
    void stopTask(uint32_t placementId);
    void resetTask(uint32_t placementId);
    void ignoreGroup(uint32_t placementId, std::string const& group);
    void clearIgnored(uint32_t placementId);
    void toggleHighlight(uint32_t placementId, VerificationStatus);
    [[nodiscard]] VerificationReport report(uint32_t placementId, BlockPos const& near, size_t limit = 32) const;

private:
    
    bool evaluate(int dimension, BlockPos const& pos, VerificationCell& cell);
    VerificationStatus compare(Block const& expected, Block const& actual);
    
    void insert(placement::PlacementInstance const&, BlockPos, Block const*);
    
    void                                                             update(util::WorldBlockKey);
    void tickTasks(util::WorkBudget const&);
    void publishOverlay(VerificationTask const&, BlockPos const&, VerificationCell const&);
    void rebuildOverlay();
    VerifierState&                                                   mState;
    placement::PlacementState const&                                 mPlacements;
    editor::ViewState const& mView;
    std::unordered_map<uint32_t, VerificationTask> mTasks;
    uint32_t mHighlightTask = 0;
    uint64_t mOverlayEpoch = 1;
    bool mTaskFirst = false;
    size_t mTaskTurn = 0, mOverlayCursor = 0;
    bool mOverlayRebuild = false;
    std::deque<std::shared_ptr<VerifierState>> mRetiredOverlays;
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
