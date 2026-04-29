#pragma once

#include "levischematic/verifier/VerifierService.h"

#include "mc/world/level/LevelListener.h"

namespace levischematic::verifier_block_listener {

class VerifierBlockListener : public LevelListener {
public:
    explicit VerifierBlockListener(levischematic::verifier::VerifierService& verifierService)
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
    levischematic::verifier::VerifierService& mVerifierService;
};

} // namespace levischematic::verifier_block_listener
