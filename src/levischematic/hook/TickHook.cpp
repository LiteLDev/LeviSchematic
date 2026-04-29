#include "TickHook.h"

#include "levischematic/app/AppKernel.h"
#include "levischematic/LeviSchematic.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/gui/screens/InGamePlayScreen.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/scripting/ServerScriptManager.h"
#include "mc/server/ServerInstance.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"

#include <vector>

namespace levischematic::hook {

namespace {

bool gTickHooksRegistered = false;

    auto& getLogger() {
    return levischematic::LeviSchematic::getInstance().getSelf().getLogger();
}

} // namespace

LL_AUTO_TYPE_INSTANCE_HOOK(
    ServerStartCommandRegistration,
    HookPriority::Highest,
    ServerScriptManager,
    &ServerScriptManager::$onServerThreadStarted,
    EventResult,
    ::ServerInstance& ins
) {
    auto res = origin(ins);
    if(!app::hasAppKernel()) {
        app::load();
    }
    app::start();

    if (app::hasAppKernel()) {
        app::getAppKernel().onServerThreadStarted();
    }

    return res;
}

LL_AUTO_TYPE_INSTANCE_HOOK(
    ServerStop,
    HookPriority::Highest,
    ServerInstance,
    &ServerInstance::startLeaveGame,
    void
) {
    origin();
    app::stop();
}

LL_TYPE_INSTANCE_HOOK(
    RenderLevelHook,
    HookPriority::Highest,
    ScreenView,
    &ScreenView::render,
    void,
    ::UIRenderContext& uiRenderContext
) {
    origin(uiRenderContext);
    MinecraftUIRenderContext& screenContext = reinterpret_cast<MinecraftUIRenderContext&>(uiRenderContext);

    auto client = ll::service::getClientInstance();
    if (!client) {
        return;
    }

    auto* player = client->getLocalPlayer();
    if (!player || !app::hasAppKernel()) {
        return;
    }

    auto overlayView = app::getAppKernel().infoOverlay().buildView(*player);
    if (!overlayView) {
        return;
    }

    auto& guiData      = screenContext.mScreenContext.guiData;
    float screenHeight = guiData->mScreenSizeData->clientScreenSize->y / guiData->getGuiScale();

    constexpr float margin     = 4.0f;
    constexpr float lineHeight = 9.0f;

    for (size_t i = 0; i < overlayView->lines.size(); ++i) {
        client->getFontHandle().mDefaultFont->drawShadow(
            screenContext.mScreenContext,
            overlayView->lines[i],
            margin,
            screenHeight - lineHeight - margin - (lineHeight * i),
            mce::Color::WHITE(),
            false,
            nullptr,
            1.0f
        );
    }
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerFireDimensionChangedHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::$_fireDimensionChanged,
    void
) {
    origin();

    if (app::hasAppKernel()) {
        app::getAppKernel().verifier().handleDimensionChanged();
    }
}

using TickHookRegistrar = ll::memory::HookRegistrar<
    ServerStartCommandRegistration,
    RenderLevelHook,
    LocalPlayerFireDimensionChangedHook>;

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

} // namespace levischematic::hook
