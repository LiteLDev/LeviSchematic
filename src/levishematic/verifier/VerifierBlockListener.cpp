#include "VerifierBlockListener.h"

#include "levishematic/LeviShematic.h"

#include "mc/world/level/BlockSource.h"

namespace levishematic::verifier_block_listener {

    auto& getLogger() {
    return levishematic::LeviShematic::getInstance().getSelf().getLogger();
}

void VerifierBlockListener::onBlockChanged(
    ::BlockSource&                 source,
    ::BlockPos const&              pos,
    uint                           /*layer*/,
    ::Block const&                 block,
    ::Block const&                 /*oldBlock*/,
    int                            /*updateFlags*/,
    ::ActorBlockSyncMessage const* /*syncMsg*/,
    ::BlockChangedEventTarget      /*eventTarget*/,
    ::Actor*                       /*blockChangeSource*/
) {
    mVerifierService.handleBlockChanged(source, pos, block);
    getLogger().debug("Block change");
}

} // namespace levishematic::verifier_block_listener
