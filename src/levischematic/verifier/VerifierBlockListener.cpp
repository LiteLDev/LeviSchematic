#include "VerifierBlockListener.h"

#include "levischematic/LeviSchematic.h"
#include "levischematic/app/AppKernel.h"

#include "mc/world/level/BlockSource.h"

namespace levischematic::verifier_block_listener {




auto& getLogger() { return levischematic::LeviSchematic::getInstance().getSelf().getLogger(); }

void VerifierBlockListener::onBlockChanged(
    ::BlockSource&    source,
    ::BlockPos const& pos,
    uint ,
    ::Block const& block,
    ::Block const& ,
    int ,
    ::ActorBlockSyncMessage const* ,
    ::BlockChangedEventTarget ,
    ::Actor* 
) {
    auto keepAlive = shared_from_this();
    auto session = app::accessSession();
    if (!session || !session->verifier().ownsListener(*this)) {
        return;
    }
    session->verifier().handleBlockChanged(source, pos, block);
    
}

void VerifierBlockListener::onSourceDestroyed(::BlockSource& source) {
    auto keepAlive = shared_from_this();
    app::sourceDestroyed(*this, source);
}

} 
