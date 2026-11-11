#include "WorldInteractionHooks.h"
#include "levischematic/worldedit/DataPick.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/network/packet/GuiDataPickItemPacket.h"
namespace levischematic::hook {
namespace {
bool registered = false;
}


LL_TYPE_INSTANCE_HOOK(
    DataPickReplyHook,
    ll::memory::HookPriority::Normal,
    ClientNetworkHandler,
    ll::memory::SymbolView{"?handle@ClientNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVGuiDataPickItemPacket@@@Z"},
    void,
    NetworkIdentifier const&     source,
    GuiDataPickItemPacket const& packet
) {
    origin(source, packet);
    worldedit::DataPick::received(packet.mSlot);
}
void registerWorldInteractionHooks() {
    if (registered) return;
    auto entry =
        ll::memory::SymbolView{
            "?handle@ClientNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVGuiDataPickItemPacket@@@Z"
        }
            .resolve(true);
    worldedit::DataPick::available = entry && DataPickReplyHook::hook() == 0;
    registered                     = true;
}
void unregisterWorldInteractionHooks() {
    if (!registered) return;
    if (worldedit::DataPick::available.exchange(false)) DataPickReplyHook::unhook();
    registered = false;
}
} 
