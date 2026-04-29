#include "InfoTextHook.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/gui/Font.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/GuiData.h"


namespace levischematic::hook {

//     std::string g_overlayText = "test text";
// LL_TYPE_INSTANCE_HOOK(
//     InfoTextHook,
//     HookPriority::Normal,
//     ScreenView,
//     &ScreenView::render,
//     void,
//     ::UIRenderContext& uiRenderContext
// ) {
//     origin(uiRenderContext);

//     MinecraftUIRenderContext& screenContext = reinterpret_cast<MinecraftUIRenderContext&>(uiRenderContext);

//     // 通过 GuiData 获取实际 UI 屏幕尺寸（会随窗口大小变化自动更新）
//     auto& guiData = *screenContext.mScreenContext.guiData;

//     // float screenWidth  = guiData.mScreenSizeData->clientScreenSize->x;   // UI 坐标系宽度
//     float screenHeight = guiData.mScreenSizeData->clientScreenSize->y;   // UI 坐标系高度

//     // 左下角：x=2（留一点边距），y = 屏幕高度 - 字体高度 - 边距
//     constexpr float margin = 4.0f;
//     constexpr float lineHeight = 9.0f; // MC 默认字体高度约为 9

//     ll::service::getClientInstance()->getFontHandle().mDefaultFont->drawShadow(
//         screenContext.mScreenContext,
//         g_overlayText,
//         margin,
//         screenHeight - lineHeight - margin,  // 左下角
//         mce::Color::WHITE(),
//         false,
//         nullptr,
//         1.0f
//     );
// }

// using InfoTextHooks = ll::memory::HookRegistrar<
//     InfoTextHook>;

// void registerInfoTextHooks() {
//     InfoTextHooks::hook();
// }

// void unregisterInfoTextHooks() {
//     InfoTextHooks::unhook();
// }
} // namespace levischematic::hook
