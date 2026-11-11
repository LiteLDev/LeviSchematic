#pragma once

#include <memory>

#include "mc/world/level/LevelListener.h"

namespace levischematic::verifier_block_listener {





class VerifierBlockListener : public LevelListener, public std::enable_shared_from_this<VerifierBlockListener> {
public:
    











    virtual void onBlockChanged(
        ::BlockSource&                 source,
        ::BlockPos const&              pos,
        uint                           layer,
        ::Block const&                 block,
        ::Block const&                 oldBlock,
        int                            updateFlags,
        ::ActorBlockSyncMessage const* syncMsg,
        ::BlockChangedEventTarget      eventTarget,
        ::Actor*                       blockChangeSource
    );
    



    virtual void onSourceDestroyed(::BlockSource& source);
};

} 
