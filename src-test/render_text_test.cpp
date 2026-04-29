
// #include "levischematic/LeviSchematic.h"

// #include "ll/api/memory/Hook.h"
// #include "ll/api/service/TargetedBedrock.h"

// #include "mc/client/gui/Font.h"
// #include "mc/client/gui/screens/ScreenContext.h"
// #include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
// #include "mc/client/gui/screens/ScreenView.h"
// #include "mc/client/game/ClientInstance.h"
// #include "mc/client/gui/FontHandle.h"



// namespace levischematic::rendertext_test {

//     auto& logger = levischematic::LeviShematic::getInstance().getSelf().getLogger();

// static std::string g_overlayText  = "§cHello§r Custom\n§aText"; // §c红色 §a绿色



// LL_AUTO_TYPE_INSTANCE_HOOK(
//     AfterScreenRenderEventHook,
//     HookPriority::Normal,
//     ScreenView,
//     &ScreenView::render,
//     void,
//     ::UIRenderContext& uiRenderContext
// ) {
//     origin(uiRenderContext);

//     MinecraftUIRenderContext& screenContext = reinterpret_cast<MinecraftUIRenderContext&>(uiRenderContext);


//     ll::service::getClientInstance()->getFontHandle().mDefaultFont->drawShadow(
//         screenContext.mScreenContext,
//         g_overlayText,
//         2,
//         screenContext.mScreenContext.ym + 300.0f,
//         mce::Color::WHITE(),
//         false,
//         nullptr,
//         1.0f
//     );
// }

// } // namespace levischematic::rendertext_test
