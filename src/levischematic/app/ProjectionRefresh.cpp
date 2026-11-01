#include "ProjectionRefresh.h"
#include "levischematic/app/AppKernel.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/dimension/Dimension.h"

namespace levischematic::app {
void refreshProjectionState(AppKernel& session, BlockSource* source) {
    if (source) session.verifier().refresh(*source);
    else session.verifier().refresh();
}
void refreshProjectionStateForDimension(AppKernel& session, Dimension* dimension) {
    refreshProjectionState(session, dimension ? &dimension->getBlockSourceFromMainChunkSource() : nullptr);
}
void refreshCurrentClientProjectionState(AppKernel& session) {
    auto  client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    refreshProjectionStateForDimension(session, player ? &player->getDimension() : nullptr);
}
} 
