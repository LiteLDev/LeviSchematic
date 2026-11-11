#pragma once
#include "BlockData.h"
#include "Task.h"
#include <string>
class LocalPlayer;
namespace levischematic::worldedit {
struct Target {
    BlockPos                position;
    Block const*            block = nullptr;
    ItemStack               item;
    std::optional<BlockPos> pickSource;
    bool                    sourceDataUnknown = false;
};

class PlayerInteraction {
    enum class Stage { Select, Pick, Inventory, World } mStage = Stage::Select;
    uint64_t                mStarted = 0, mMatched = 0;
    bool                    mDestroy    = false;
    uint64_t                mPickTicket = 0;
    std::optional<BlockPos> mDestroyAt;

public:
    std::string detail;
    bool        pending() const { return mStage != Stage::Select; }
    void        reset() {
        mStage   = Stage::Select;
        mStarted = mMatched = 0;
        mDestroy            = false;
        mDestroyAt.reset();
        detail.clear();
    }
    Outcome advance(LocalPlayer&, Target&, uint64_t now);
    
    Outcome acquireSourceItem(LocalPlayer&, Target&, uint64_t now);
    void    stop(LocalPlayer&);
};
} 
