#include "TickHook.h"

#include "levischematic/LeviSchematic.h"
#include "levischematic/app/AppKernel.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/MinecraftGame.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/GameCallbacks.h"


namespace levischematic::hook {

namespace {

bool gTickHooksRegistered = false;

} 








LL_TYPE_INSTANCE_HOOK(
    ClientStartLeaveGameHook,
    HookPriority::Highest,
    ClientInstance,
    &ClientInstance::_startLeaveGame,
    void
) {
    
    app::clientLeaving(*this);
    origin();
}





LL_TYPE_INSTANCE_HOOK(
    ClientLevelExitHook,
    HookPriority::Highest,
    ClientInstance,
    &ClientInstance::$onLevelExit,
    void
) {
    
    
    
    
    
    app::clientLeaving(*thisFor<GameCallbacks>());
    origin();
}





LL_TYPE_INSTANCE_HOOK(
    LocalPlayerFireDimensionChangedHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::$_fireDimensionChanged,
    void
) {
    app::invalidateRuntime();
    origin();
}







LL_TYPE_INSTANCE_HOOK(
    BeginResourceReloadHook,
    HookPriority::Highest,
    MinecraftGame,
    &MinecraftGame::_beginReloadingResources,
    void
) {
    app::beginResourceReload();
    origin();
}








LL_TYPE_INSTANCE_HOOK(
    QueueResourceReloadHook,
    HookPriority::Highest,
    MinecraftGame,
    &MinecraftGame::_finishReloadingResources,
    void,
    ResourcePackManager& manager,
    bool                 fullVanillaPackOnStack
) {
    origin(manager, fullVanillaPackOnStack);
    app::resourceReloadQueued();
}







LL_TYPE_INSTANCE_HOOK(
    CleanReloadMaterialsHook,
    HookPriority::Highest,
    MinecraftGame,
    &MinecraftGame::cleanReloadMaterials,
    void
) {
    app::beginResourceReload();
    origin();
    app::resourceReloadQueued();
}

using TickHookRegistrar = ll::memory::HookRegistrar<
    ClientStartLeaveGameHook,
    ClientLevelExitHook,
    LocalPlayerFireDimensionChangedHook,
    BeginResourceReloadHook,
    QueueResourceReloadHook,
    CleanReloadMaterialsHook>;

void registerTickHooks() {
    if (gTickHooksRegistered) {
        return;
    }

    TickHookRegistrar::hook();
    gTickHooksRegistered = true;
}

void unregisterTickHooks() {
    if (!gTickHooksRegistered) {
        return;
    }

    TickHookRegistrar::unhook();
    gTickHooksRegistered = false;
}

} 
