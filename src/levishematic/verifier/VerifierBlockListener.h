#pragma once

#include "levishematic/verifier/VerifierService.h"

#include "mc/world/level/LevelListener.h"

namespace levishematic::verifier_block_listener {

class VerifierBlockListener : public LevelListener {
public:
    explicit VerifierBlockListener(levishematic::verifier::VerifierService& verifierService)
        : mVerifierService(verifierService) {}

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

private:
    levishematic::verifier::VerifierService& mVerifierService;
};

} // namespace levishematic::verifier_block_listener
