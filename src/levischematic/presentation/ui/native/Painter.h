#pragma once
#include "levischematic/presentation/ui/layout/UiLayout.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/gui/controls/UIRenderContext.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTextureData.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
namespace levischematic::ui {
inline RectangleArea area(UiRect r) { return {r.x, r.x + r.width, r.y, r.y + r.height}; }
struct Painter {
    UIRenderContext& ctx;
    Font&            font;
    UiRect           clip{0, 0, 100000, 100000};
    float            oldAlpha;
    Painter(UIRenderContext& c, Font& f) : ctx(c), font(f), oldAlpha(c.getTextAlpha()) {
        ctx.saveCurrentClippingRectangle();
        ctx.setFullClippingRectangle();
        ctx.setTextAlpha(1);
    }
    ~Painter() {
        ctx.flushText(0, std::nullopt);
        ctx.disableScissorTest();
        ctx.restoreSavedClippingRectangle();
        ctx.setTextAlpha(oldAlpha);
    }
    float measure(std::string const& text) { return static_cast<float>(ctx.getLineLength(font, text, 1, false)); }
    void  fill(UiRect r, mce::Color color) {
        r = intersect(r, clip);
        if (r.width <= 0 || r.height <= 0) return;
        ctx.fillRectangle(area(r), color, color.a);
        static HashedString const material{"ui_fillColor"};
        ctx.flushImages(mce::Color{1.f, 1.f, 1.f, 1.f}, 1, material);
    }
    void outline(UiRect r, mce::Color c) {
        fill({r.x, r.y, r.width, 1}, c);
        fill({r.x, r.y + r.height - 1, r.width, 1}, c);
        fill({r.x, r.y, 1, r.height}, c);
        fill({r.x + r.width - 1, r.y, 1, r.height}, c);
    }
    void sprite(mce::TexturePtr const& texture, UiRect r, UiRect uv, float sourceWidth, float sourceHeight) {
        auto visible = intersect(r, clip);
        if (visible.width <= 0 || visible.height <= 0 || r.width <= 0 || r.height <= 0) return;
        uv = {
            uv.x + (visible.x - r.x) / r.width * uv.width,
            uv.y + (visible.y - r.y) / r.height * uv.height,
            uv.width * visible.width / r.width,
            uv.height * visible.height / r.height
        };
        r                = visible;
        auto const& data = texture.mClientTexture.get();
        if (!data) return;
        ctx.drawImage(
            data->mClientTexture,
            {r.x, r.y},
            {r.width, r.height},
            {uv.x / sourceWidth, uv.y / sourceHeight},
            {uv.width / sourceWidth, uv.height / sourceHeight},
            false
        );
        static HashedString const material{"ui_flush"};
        ctx.flushImages(mce::Color{1.f, 1.f, 1.f, 1.f}, 1, material);
    }
    void button(mce::TexturePtr const& texture, UiRect r, float border = 3) {

        float xs[] = {r.x, r.x + border, r.x + r.width - border, r.x + r.width};
        float ys[] = {r.y, r.y + border, r.y + r.height - border, r.y + r.height};
        float us[] = {0, border, 200 - border, 200}, vs[] = {0, border, 20 - border, 20};
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x)
                sprite(
                    texture,
                    {xs[x], ys[y], xs[x + 1] - xs[x], ys[y + 1] - ys[y]},
                    {us[x], vs[y], us[x + 1] - us[x], vs[y + 1] - vs[y]},
                    200,
                    20
                );
    }
    void text(std::string const& value, UiRect r, bool center = false, mce::Color color = {1.f, 1.f, 1.f, 1.f}) {
        auto                   align = ::ui::TextAlignment::Left;
        TextMeasureData const  data{1.f, 0.f, true, false, false, align};
        CaretMeasureData const caret{-1, false};
        for (auto const& run : layoutTextRuns(value, r, clip, [&](auto const& s) { return measure(s); }, center)) {
            ctx.drawText(font, area(run.rect), std::string(run.text), color, 1, align, data, caret);
            ctx.flushText(0, std::nullopt);
        }
    }
};
}
