#include "VerifierBlockListener.h"

#include "levischematic/LeviSchematic.h"

#include "mc/world/level/BlockSource.h"

namespace levischematic::verifier_block_listener {

    auto& getLogger() {
    return levischematic::LeviSchematic::getInstance().getSelf().getLogger();
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
    // getLogger().debug("Block change");
}

} // namespace levischematic::verifier_block_listener
