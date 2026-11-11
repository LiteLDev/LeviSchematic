#include "ProjectionHudHook.h"
#include "levischematic/app/AppKernel.h"
#include "levischematic/app/ProjectionHud.h"
#include "levischematic/ui/UiI18n.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/gui/controls/UIRenderContext.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/VanillaItemNames.h"

namespace levischematic::hook {
namespace {
bool registered = false;

RectangleArea rectangle(float x, float y, float width, float height) { return {x, x + width, y, y + height}; }


struct HudRenderState {
    UIRenderContext& context;
    float            alpha;
    explicit HudRenderState(UIRenderContext& ctx) : context(ctx), alpha(ctx.getTextAlpha()) {
        context.saveCurrentClippingRectangle();
        context.setFullClippingRectangle();
        context.setTextAlpha(1);
    }
    ~HudRenderState() {
        context.flushText(0, std::nullopt);
        context.disableScissorTest();
        context.setTextAlpha(alpha);
        context.restoreSavedClippingRectangle();
    }
};
} 






LL_TYPE_INSTANCE_HOOK(
    ProjectionHudRenderHook,
    ll::memory::HookPriority::Normal,
    ScreenView,
    &ScreenView::render,
    void,
    UIRenderContext& context
) {
    origin(context);
    auto const name = getScreenName();
    if (name != "hud_screen" && name != "hud.hud_screen") return;
    auto client = ll::service::getClientInstance();
    if (!client || !client->isReadyToRender() || client->isShowingMenu() || client->getOptions().getHideHud()
        || client->getOptions().getHideScreens())
        return;
    auto* player = client->getLocalPlayer();
    if (!player) return;
    drawProjectionHuds(context, app::configStore().snapshot().hud);
}

ui::HudCanvas drawProjectionHuds(UIRenderContext& context, app::Config::Hud const& config, bool preview, int selected, bool paint) {
    ui::HudCanvas canvas;
    auto session = app::tryAccessSession();
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    
    
    
    if (!session || !session->hudReady() || !player) return canvas;
    std::array<std::vector<std::string>, 4> panels;
    auto& lines = panels[0];
    if (player->getSelectedItem().isInstance(VanillaItemNames::Stick(), false)) {
        auto snapshot = app::projectionHudSnapshot(*session);
        if (snapshot) lines = ui::projectionHudLines(*snapshot, ui::tr);
    }
    auto extra = session->survival().hudLines();
    auto split = std::min(extra.size(), session->survival().targetHudLineCount());
    panels[1].assign(extra.begin(), extra.begin() + split);
    panels[2].assign(extra.begin() + split, extra.end());
    if (app::configStore().snapshot().survival.verifierHud) {
        auto selected = session->placement().selectedPlacementId();
        if (selected) {
            auto report = session->verifier().report(*selected, BlockPos(player->getPosition()), 3);
            panels[3].push_back(report.running ? "Verifier / 校验中" : "Verifier / 校验已停止");
            for (auto const& location : report.nearby)
                panels[3].push_back(
                    std::string(verifier::verificationStatusLabel(location.status)) + ": "
                    + std::to_string(location.position.x) + ", " + std::to_string(location.position.y) + ", "
                    + std::to_string(location.position.z)
                );
        }
    }

    auto  fontHandle = client->getFontHandle();
    auto& font       = *fontHandle.mDefaultFont;
    auto  measure    = [&](std::string const& text) {
        return static_cast<float>(context.getLineLength(font, text, 1, false));
    };
    auto area = context.getFullClippingRectangle();
    
    
    
    auto        gui        = client->getGuiData();
    auto const& clientSize = gui->mScreenSizeData->clientScreenSize.get();
    if (!gui->mScreenSizeDataValid || clientSize.x <= 0 || clientSize.y <= 0) return canvas;
    auto  safe     = gui->getSafeScreenZoneArea();
    float scaleX   = (area._x1 - area._x0) / clientSize.x;
    float scaleY   = (area._y1 - area._y0) / clientSize.y;
    float left     = std::max(area._x0, area._x0 + safe._x0 * scaleX);
    float top      = std::max(area._y0, area._y0 + safe._y0 * scaleY);
    float right    = std::min(area._x1, area._x0 + safe._x1 * scaleX);
    float bottom   = std::min(area._y1, area._y0 + safe._y1 * scaleY);
    canvas.safe = {left + 8, top + 8, std::max(0.f, right - left - 16), std::max(0.f, bottom - top - 16)};
    if (canvas.safe.width < 16 || canvas.safe.height < 16) return canvas;
    static char const* titles[] = {"投影信息 / Projection", "目标方块 / Target", "材料 / Materials", "校验 / Verifier"};
    float stackedBottom = std::max(canvas.safe.y, bottom - 48);
    float columnOffset = 0, columnWidth = 0;
    HudRenderState state(context);
    for (size_t i = 0; i < panels.size(); ++i) {
        auto& content = panels[i];
        if (preview) {
            if (content.empty()) content = {ui::localizedText("示例 / Example"), "X: 0  Y: 64  Z: 0"};
            content.insert(content.begin(), ui::localizedText(titles[i]));
        }
        if (content.empty()) continue;
        float scale = config.panels[i].scalePercent / 100.f;
        float padding = 4 * scale;
        float lineHeight = (std::ceil(font.getWrapHeight()) + 2) * scale;
        float maxWidth = 0;
        for (auto const& line : content) maxWidth = std::max(maxWidth, measure(line) * scale);
        float panelWidth = std::min(maxWidth + 2 * padding, canvas.safe.width * .45f);
        float panelHeight = lineHeight * content.size() + 2 * padding;
        if (config.panels[i].x < 0 && stackedBottom - panelHeight < canvas.safe.y && columnWidth > 0) {
            columnOffset += columnWidth + 4;
            columnWidth = 0;
            stackedBottom = canvas.safe.y + canvas.safe.height;
        }
        auto r = ui::placeHud(config.panels[i], canvas.safe, panelWidth, panelHeight,
            config.position == "bottom_right", stackedBottom);
        canvas.panels[i] = r;
        if (config.panels[i].x < 0) {
            r.x += config.position == "bottom_right" ? -columnOffset : columnOffset;
            r.x = std::clamp(r.x, canvas.safe.x, canvas.safe.x + canvas.safe.width - r.width);
            canvas.panels[i] = r;
            columnWidth = std::max(columnWidth, r.width);
            stackedBottom = r.y - 4;
        }
        if (!paint) continue;
        context.fillRectangle(rectangle(r.x, r.y, r.width, r.height), mce::Color{0.f, 0.f, 0.f, 1.f}, .55f);
        if (preview) {
            auto color = selected == static_cast<int>(i) ? mce::Color{1.f, .85f, .2f, 1.f} : mce::Color{.7f, .7f, .7f, 1.f};
            for (auto edge : {ui::UiRect{r.x, r.y, r.width, 1}, {r.x, r.y + r.height - 1, r.width, 1},
                             {r.x, r.y, 1, r.height}, {r.x + r.width - 1, r.y, 1, r.height}})
                context.fillRectangle(rectangle(edge.x, edge.y, edge.width, edge.height), color, 1.f);
        }
        context.setClippingRectangle(rectangle(r.x, r.y, r.width, r.height));
        context.enableScissorTest(rectangle(r.x, r.y, r.width, r.height));
        TextMeasureData const textData{scale, 0.f, false, false, false, ::ui::TextAlignment::Left};
        CaretMeasureData const caretData{-1, false};
        float y = r.y + padding;
        for (auto const& line : content) {
            context.drawText(font, rectangle(r.x + padding, y, std::max(0.f, r.width - 2 * padding), lineHeight),
                ui::fitHudText(line, std::max(0.f, r.width - 2 * padding) / scale, measure),
                mce::Color{143.f / 255, 191.f / 255, 135.f / 255, 1.f}, 1,
                ::ui::TextAlignment::Left, textData, caretData);
            y += lineHeight;
        }
        context.flushText(0, std::nullopt);
        context.disableScissorTest();
        context.setFullClippingRectangle();
    }
    return canvas;
}

void registerProjectionHudHooks() {
    if (registered) return;
    ProjectionHudRenderHook::hook();
    registered = true;
}
void unregisterProjectionHudHooks() {
    if (!registered) return;
    ProjectionHudRenderHook::unhook();
    registered = false;
}
} 
