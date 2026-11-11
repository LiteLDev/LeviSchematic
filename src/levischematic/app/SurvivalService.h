#pragma once
#include "MaterialService.h"
#include "levischematic/survival/InventoryAccess.h"
#include "mc/world/phys/HitResult.h"
class IClientInstance;
namespace levischematic::app {

class SurvivalService {
public:
    explicit SurvivalService(PlacementService& placements) : mPlacements(placements), mMaterials(placements) {}
    MaterialService& materials() { return mMaterials; }
    void             tick(IClientInstance&, util::WorkBudget const&);
    void             reset();
    bool             hasTarget(IClientInstance&);
    void             pick(IClientInstance&);
    void             use(bool held) {
        mUseHeld = held;
        if (held) mUseRequested = true;
    }
    void                     info(bool held) { mInfoHeld = held; }
    bool                     restrictPlacement(LocalPlayer&, BlockPos const&, unsigned char face);
    std::vector<std::string> hudLines() const { return mHud; }
    size_t targetHudLineCount() const { return mTargetHudLines; }
    std::string const&       message() const { return mMessage; }

private:
    std::optional<placement::ProjectionHit> target(LocalPlayer&);
    void                                    notify(LocalPlayer&, std::string);
    void                                    advance(LocalPlayer&, uint64_t now);
    PlacementService&                       mPlacements;
    MaterialService                         mMaterials;
    survival::Operation                     mOperation;
    std::optional<BlockPos>                 mTarget;
    uint32_t                                mOwner = 0;
    uint64_t                 mPlacementRevision = 0, mSwapTime = 0, mNextUse = 0, mLastMessage = 0, mNextHud = 0;
    int                      mDimension = INT32_MIN;
    bool                     mUseHeld = false, mUseRequested = false, mInfoHeld = false;
    bool                     mObservedMatch = false;
    uint64_t                 mLastProbe     = 0;
    std::string              mMessage;
    std::vector<std::string> mHud;
    size_t mTargetHudLines = 0;
};
} 
