#include "SurvivalHooks.h"
#include "levischematic/app/AppKernel.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerPlaceBlockEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/item/Item.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/dimension/Dimension.h"
namespace levischematic::hook {
namespace {
ll::event::ListenerPtr placing;
bool                   registered = false;
bool                   blocksBuild(GameMode& mode, BlockPos const& clicked, uchar face, HandSlot hand) {
    if (hand != HandSlot::Mainhand || face >= 6 || !app::configStore().snapshot().survival.placementRestriction)
        return false;
    auto client = ll::service::getClientInstance();
    auto player = client ? client->getLocalPlayer() : nullptr;
    if (!player || &mode.mPlayer != player) return false;
    auto const& held = player->getSelectedItem();
    if (!held || !held.getBlockForRendering()) return false;
    auto& source = player->getDimension().getBlockSourceFromMainChunkSource();
    
    if (!player->isSneaking() && source.getBlock(clicked).getBlockType().isInteractiveBlock()) return false;
    auto pos = clicked;
    if (!held.mItem->_calculatePlacePos(held, *player, face, pos)) return false;
    auto session = app::accessSession();
    return session && !session->worldEdit().submitting() && session->survival().restrictPlacement(*player, pos, face);
}
} 


LL_TYPE_INSTANCE_HOOK(
    RestrictedBuildHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$buildBlock,
    bool,
    BlockPos const& pos,
    uchar           face,
    HandSlot        hand,
    bool const      simTick
) {
    if (blocksBuild(*this, pos, face, hand)) return false;
    return origin(pos, face, hand, simTick);
}
using BuildHooks = ll::memory::HookRegistrar<RestrictedBuildHook>;
void registerSurvivalHooks() {
    if (registered) return;
    BuildHooks::hook();
    registered = true;
    
    
    placing = ll::event::EventBus::getInstance().emplaceListener<ll::event::PlayerPlacingBlockEvent>([](auto& event) {
        if (event.isCancelled()) return;
        auto client = ll::service::getClientInstance();
        auto player = client ? client->getLocalPlayer() : nullptr;
        if (!player || &event.self() != player) return;
        auto session = app::accessSession();
        if (session && !session->worldEdit().submitting()
            && session->survival().restrictPlacement(*player, event.pos(), event.face()))
            event.cancel();
    });
}
void unregisterSurvivalHooks() {
    if (!registered) return;
    if (placing) ll::event::EventBus::getInstance().removeListener<ll::event::PlayerPlacingBlockEvent>(placing);
    placing.reset();
    BuildHooks::unhook();
    registered = false;
}
} 
