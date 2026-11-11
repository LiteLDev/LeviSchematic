#include "Bindings.h"
#include "OpenRequests.h"
#include "Ui.h"
#include "UiI18n.h"
#include "UiLayout.h"
#include "UiTextEdit.h"
#include "levischematic/LeviSchematic.h"
#include "levischematic/app/AppKernel.h"
#include "levischematic/hook/ProjectionHudHook.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInputCallbacks.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/MinecraftGame.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/gui/controls/UIRenderContext.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/SceneFactory.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"
#include "mc/client/input/KeyboardManager.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTextureData.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>

namespace levischematic::ui {
namespace {
std::atomic_bool       enabled = false, opened = false, capturesText = false, capturesInput = false, canOpen = false;
std::mutex             mutex;
OpenRequests           requests;
std::string            lastEvent = "not started";
nlohmann::ordered_json lastFrame;
std::atomic_uint64_t   frameNumber = 0;
std::atomic_int        pageNumber  = static_cast<int>(PageId::MainMenu);
struct Input {
    enum Kind { Mouse, Key, Text, FocusLost } kind;
    int         code = 0, data = 0;
    float       x = 0, y = 0;
    std::string text;
    uint64_t    generation = 0;
};
std::deque<Input>                 inputs;
std::atomic_uint64_t              inputGeneration = 0;
std::atomic<float>                inverseScale    = 1;
std::array<std::atomic_bool, 256> heldKeys{};
std::array<std::atomic_bool, 8>   heldMouse{};
std::array<std::atomic_bool, 8>   mouseDown{};
ll::event::ListenerPtr            keyboardListener, mouseListener;
std::shared_ptr<AbstractScene>    scene, retired;


std::vector<std::weak_ptr<AbstractScene>> exitingScenes;
bool                              seen = false, popping = false, keyboardClaimed = false, keyboardEnabled = false;
PageId                            current           = PageId::MainMenu;
uint64_t                          sessionGeneration = 0, resourceGeneration = 0;
struct Screen {
    std::shared_ptr<Page> page;
    UiViewState           view;
};
std::map<PageId, Screen> screens;
std::vector<PageId>      history;
UiFrame                  frame;
std::optional<Widget>    focused, dragging, pressed;
UiTextEdit               edit;
bool                     shift = false, control = false;
float                    mouseX = -1, mouseY = -1;
unsigned                 unseenTicks = 0;
app::Config::Hud hudDraft;
HudCanvas hudCanvas;
int selectedHud = 0;
bool hudDirty = false, hudDragging = false;
bool hudPositionsPending = false;
float hudGrabX = 0, hudGrabY = 0;

void setHudScale(std::string text) {
    try {
        size_t end = 0;
        double value = std::stod(text, &end);
        if (end != text.size() || !std::isfinite(value) || value < 50 || value > 200 || value != std::round(value))
            throw std::out_of_range("scale");
        hudDraft.panels[selectedHud].scalePercent = static_cast<int>(value);
        hudDirty = true;
        if (!screens.empty()) screens.at(current).page->message.clear();
    } catch (...) {
        if (!screens.empty()) screens.at(current).page->message = "大小范围：50%–200% / Scale: 50%–200%";
    }
}

auto& mod() { return LeviSchematic::getInstance().getSelf(); }
void  report(std::string message) {
    {
        std::lock_guard lock(mutex);
        lastEvent = message;
    }
    mod().getLogger().info("UI: {}", message);
}
Screen& screen() { return screens.at(current); }
void    releaseKeyboard() {
    capturesText = false;
    if (auto client = ll::service::getClientInstance()) {
        auto& kb = client->getKeyboardManager();
        if (keyboardEnabled) kb.disableKeyboard();
        if (keyboardClaimed) kb.releaseKeyboardOwnership();
    }
    keyboardEnabled = keyboardClaimed = false;
}
void clearPages() {
    releaseKeyboard();
    focused.reset();
    dragging.reset();
    pressed.reset();
    hudDragging = false;
    for (auto& [id, s] : screens) {
        clearPage(*s.page);
    }
    screens.clear();
    history.clear();
    frame = {};
    ++inputGeneration;
}
Row const* rowById(Page const& p, std::string const& id) {
    if (auto r = findRow(p, id)) return r;
    for (auto const& r : p.rows)
        for (auto const& a : r.actions)
            if (a.id == id) return &a;
    return nullptr;
}
void commitEdit() {
    if (!focused || screens.empty()) return;
    auto widget = *focused;
    auto text   = edit.text;
    focused.reset();
    releaseKeyboard();
    auto p = screen().page;
    if (widget.op == UiOp::Search) {
        screen().view.search = text;
        screen().view.scroll = 0;
        return;
    }
    if (auto r = rowById(*p, widget.row)) {
        auto  callback                                     = widget.index == 1 ? r->submit2 : r->submit;
        auto& stored                                       = const_cast<Row&>(*r);
        (widget.index == 1 ? stored.value2 : stored.value) = text;
        if (callback) callback(std::move(text));
    }
}
bool savePage() {
    commitEdit();
    if (screens.empty()) return true;
    if (current == PageId::HudLayout && hudDirty) {
        auto value = app::configStore().editableSnapshot();
        value.hud = hudDraft;
        auto error = app::configStore().submit(value);
        if (error.empty()) error = app::configStore().save();
        if (!error.empty()) { screen().page->message = error; return false; }
        hudDirty = false;
    }
    auto p = screen().page;
    dismissModal(p);
    return !p->save || p->save();
}
void navigate(PageId id, bool push = true) {
    if (!screens.empty() && !savePage()) return;
    if (!screens.empty() && push && id != current) history.push_back(current);
    current    = id;
    pageNumber = static_cast<int>(id);
    if (!screens.contains(id)) screens.emplace(id, Screen{buildPage(id), {}});
    else if (id == PageId::Projection || id == PageId::Placement || id == PageId::Resources || id == PageId::Tasks || id == PageId::Settings) {
        auto& page = screens.at(id).page;
        clearPage(*page);
        page = buildPage(id);
    }
    if (id == PageId::HudLayout) {
        hudDraft = app::configStore().editableSnapshot().hud;
        hudDirty = hudDragging = false;
        hudPositionsPending = true;
        selectedHud = 0;
        auto p = screens.at(id).page;
        p->rows.clear();
        PageBuilder b{p};
        b.button("hud_panel", "选择面板 / Panel", [] { selectedHud = (selectedHud + 1) % 4; })
            .input("hud_scale", "大小 % / Scale %", "100", setHudScale)
            .input("hud_slider", "大小 / Scale", "100", setHudScale)
            .button("hud_reset", "重置此项 / Reset", [] { hudDraft.panels[selectedHud] = {}; hudDirty = hudPositionsPending = true; })
            .button("hud_reset_all", "全部重置 / Reset all", [] { hudDraft = {}; hudDirty = hudPositionsPending = true; });
        p->rows[2].kind = RowKind::Slider;
        p->rows[2].minimum = 50;
        p->rows[2].maximum = 200;
        p->rows[2].step = 1;
    }
    frame = {};
    pressed.reset();
    dragging.reset();
    ++inputGeneration;
}
void closeMenu(bool save = true) {
    if (save && !savePage()) return;
    if (!save && !screens.empty() && !savePage()) report("forced close: settings could not be saved");
    releaseKeyboard();
    if (scene && !popping)
        if (auto c = ll::service::getClientInstance(); c && c->getClientSceneStack()->getTopScene() == scene.get()) {
            popping = true;
            c->getClientSceneStack()->schedulePopScreen(1);
        }
}
void back() {
    if (screens.empty()) return;
    auto p = screen().page;
    if (p->capturing) {
        p->capturing = false;
        p->rawKey    = {};
        p->message.clear();
        return;
    }
    if (p->dismiss) {
        dismissModal(p);
        frame = {};
        return;
    }
    if (focused) {
        commitEdit();
        return;
    }
    if (history.empty()) closeMenu();
    else {
        auto parent = history.back();
        if (!savePage()) return;
        history.pop_back();
        navigate(parent, false);
    }
}
void enqueue(Input value) {
    value.generation = inputGeneration;
    std::lock_guard lock(mutex);
    if (inputs.size() < 1024) inputs.push_back(std::move(value));
}
void focus(Widget const& w) {
    commitEdit();
    focused = w;
    edit.set(w.op == UiOp::Search ? screen().view.search : w.text);
    if (auto c = ll::service::getClientInstance()) {
        auto& kb        = c->getKeyboardManager();
        keyboardClaimed = kb.tryClaimKeyboardOwnership();
        if (keyboardClaimed)
            keyboardEnabled = kb.tryEnableKeyboard(
                {},
                4096,
                false,
                false,
                false,
                Vec2{w.rect.x, w.rect.y + w.rect.height},
                w.rect.height
            );
    }
    capturesText = true;
}
std::string clipboardRead() {
    std::string result;
    if (!OpenClipboard(nullptr)) return result;
    if (auto h = GetClipboardData(CF_UNICODETEXT)) {
        if (auto p = static_cast<wchar_t const*>(GlobalLock(h))) {
            int n = WideCharToMultiByte(CP_UTF8, 0, p, -1, nullptr, 0, nullptr, nullptr);
            if (n > 1) {
                result.resize(n);
                WideCharToMultiByte(CP_UTF8, 0, p, -1, result.data(), n, nullptr, nullptr);
                result.pop_back();
            }
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return result;
}
void clipboardWrite(std::string const& value) {
    int  n = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    auto h = GlobalAlloc(GMEM_MOVEABLE, (n + 1) * sizeof(wchar_t));
    if (!h) return;
    auto p = static_cast<wchar_t*>(GlobalLock(h));
    if (!p) {
        GlobalFree(h);
        return;
    }
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), p, n);
    p[n] = 0;
    GlobalUnlock(h);
    if (!OpenClipboard(nullptr)) {
        GlobalFree(h);
        return;
    }
    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, h)) GlobalFree(h);
    CloseClipboard();
}
void invoke(Widget w, float x = 0) {
    if (screens.empty()) return;
    if (w.op == UiOp::Back) {
        back();
        return;
    }
    if (w.op == UiOp::Category) {
        if (!savePage()) return;
        if (w.index == 4) navigate(PageId::KeyBindings);
        else if (w.index == 5) navigate(PageId::Layers);
        else {
            navigate(PageId::Settings);
            screen().view.category = w.index;
            screen().view.scroll   = 0;
        }
        return;
    }
    if (w.kind == WidgetKind::Text) {
        focus(w);
        return;
    }
    commitEdit();
    auto p = screen().page;
    auto r = rowById(*p, w.row);
    if (!r) return;
    
    if (w.op == UiOp::Reset) {
        auto fn = r->reset;
        if (fn) fn();
    } else if (w.op == UiOp::Clear) {
        auto fn = r->clear;
        if (fn) fn();
    } else if (w.op == UiOp::Activate) {
        auto fn = r->activate;
        if (fn) fn();
    } else if (w.op == UiOp::Color) {
        auto fn = r->colorSubmit;
        if (fn) fn(w.index, std::clamp((x - w.rect.x) / std::max(1.f, w.rect.width - 4), 0.f, 1.f));
    } else {
        auto        fn = r->submit;
        std::string value;
        if (w.kind == WidgetKind::Slider) {
            auto v =
                sliderModelValue((x - w.rect.x) / std::max(1.f, w.rect.width - 4), r->minimum, r->maximum, r->step);
            value = std::to_string(v);
        } else if (r->kind == RowKind::Toggle) value = r->value == "true" ? "false" : "true";
        else if (w.op == UiOp::Choice) {
            int i = 0;
            try {
                i = std::stoi(r->value);
            } catch (...) {}
            value = std::to_string((i + 1) % r->choices.size());
        }
        if (r->kind == RowKind::Slider || r->kind == RowKind::Choice || r->kind == RowKind::Toggle)
            const_cast<Row&>(*r).value = value;
        if (fn) fn(std::move(value));
    }
}
void input(Input const& e) {
    if (screens.empty() || e.generation != inputGeneration) return;
    auto p = screen().page;
    if (e.kind == Input::FocusLost) {
        hudDragging = false;
        commitEdit();
        dragging.reset();
        pressed.reset();
        shift = control = false;
        p->capturing    = false;
        p->rawKey       = {};
        return;
    }
    if (e.kind == Input::Text) {
        if (focused && std::none_of(e.text.begin(), e.text.end(), [](unsigned char c) { return c < 32 || c == 127; })
            && edit.text.size() + e.text.size() <= 16384) {
            edit.insert(e.text);
            if (focused->op == UiOp::Search) {
                screen().view.search = edit.text;
                screen().view.scroll = 0;
            }
        }
        return;
    }
    if (e.kind == Input::Key) {
        if (e.code == VK_SHIFT || e.code == VK_LSHIFT || e.code == VK_RSHIFT) shift = e.data;
        if (e.code == VK_CONTROL || e.code == VK_LCONTROL || e.code == VK_RCONTROL) control = e.data;
        if (e.code == VK_ESCAPE && e.data) {
            back();
            return;
        }
        if (e.code == VK_F7 && e.data && !p->capturing && !focused) {
            closeMenu();
            return;
        }
        if (e.code == 'M' && !e.data && !p->capturing && !focused) {
            closeMenu();
            return;
        }
        if (p->capturing) {
            auto fn = p->rawKey;
            if (fn) fn(e.code, 4, e.data != 0);
            return;
        }
        if (!e.data) return;
        if (focused) {
            if (e.code == VK_RETURN) commitEdit();
            else if (e.code == VK_LEFT) edit.move(edit.previous(edit.caret), shift);
            else if (e.code == VK_RIGHT) edit.move(edit.next(edit.caret), shift);
            else if (e.code == VK_HOME) edit.move(0, shift);
            else if (e.code == VK_END) edit.move(edit.text.size(), shift);
            else if (e.code == VK_BACK) edit.erase(true);
            else if (e.code == VK_DELETE) edit.erase(false);
            else if (control && e.code == 'A') edit.selectAll();
            else if (control && e.code == 'C') clipboardWrite(edit.selection());
            else if (control && e.code == 'X') {
                clipboardWrite(edit.selection());
                edit.insert({});
            } else if (control && e.code == 'V') {
                auto text = clipboardRead();
                std::erase(text, '\r');
                std::erase(text, '\n');
                if (text.size() + edit.text.size() <= 16384) edit.insert(text);
            }
            if (focused && focused->op == UiOp::Search) {
                screen().view.search = edit.text;
                screen().view.scroll = 0;
            }
        } else if (e.code == VK_NEXT || e.code == VK_DOWN)
            screen().view.scroll += e.code == VK_NEXT ? frame.body.height : 22;
        else if (e.code == VK_PRIOR || e.code == VK_UP)
            screen().view.scroll -= e.code == VK_PRIOR ? frame.body.height : 22;
        return;
    }
    
    if (e.code != 4) {
        mouseX = e.x;
        mouseY = e.y;
    }
    if (e.code == 1) {
        auto            hit = frame.hit(e.x, e.y);
        std::lock_guard lock(mutex);
        lastEvent = "pointer " + std::to_string(e.data) + " " + std::to_string(e.x) + "," + std::to_string(e.y)
                  + " hit=" + (hit ? hit->id : "none");
    }
    if (p->capturing && e.code != 0) {
        auto fn = p->rawKey;
        if (fn) fn(e.code == 4 ? 0 : e.code, e.code == 4 ? (e.data > 0 ? 2 : 3) : 1, e.data != 0);
        return;
    }
    if (e.code == 4) {
        if (frame.body.contains(mouseX, mouseY)) {
            commitEdit();
            screen().view.scroll -= e.data > 0 ? 44 : -44;
        }
        return;
    }
    if (current == PageId::HudLayout) {
        if (e.code == 0 && hudDragging) {
            moveHud(hudDraft.panels[selectedHud], hudCanvas.safe, hudCanvas.panels[selectedHud],
                    e.x - hudGrabX, e.y - hudGrabY);
            hudDirty = true;
            return;
        }
        if (e.code == 1 && !e.data && hudDragging) {
            hudDragging = false;
            return;
        }
        if (e.code == 1 && e.data && !frame.hit(e.x, e.y)) {
            for (int j = -1; j < 4; ++j) {
                int i = j < 0 ? selectedHud : 3 - j;
                if (j >= 0 && i == selectedHud) continue;
                auto r = hudCanvas.panels[i];
                if (!r.contains(e.x, e.y)) continue;
                commitEdit();
                selectedHud = i;
                hudGrabX = e.x - r.x;
                hudGrabY = e.y - r.y;
                hudDragging = true;
                return;
            }
        }
    }
    if (e.code == 0 && dragging) {
        if (dragging->op == UiOp::Scroll)
            screen().view.scroll = std::clamp(
                                       (e.y - frame.body.y - dragging->rect.height / 2)
                                           / std::max(1.f, frame.body.height - dragging->rect.height),
                                       0.f,
                                       1.f
                                   )
                                 * frame.maxScroll;
        else invoke(*dragging, e.x);
    }
    if (e.code != 1) return;
    if (e.data) {
        if (auto w = frame.hit(e.x, e.y)) {
            pressed = *w;
            if (w->kind == WidgetKind::Slider || w->kind == WidgetKind::Scrollbar) {
                commitEdit();
                dragging = *w;
                if (w->kind == WidgetKind::Slider) invoke(*w, e.x);
            }
        } else commitEdit();
    } else {
        if (pressed && !dragging)
            if (auto w = frame.hit(e.x, e.y); w && w->id == pressed->id) invoke(*w, e.x);
        pressed.reset();
        dragging.reset();
    }
}

RectangleArea area(UiRect r) { return {r.x, r.x + r.width, r.y, r.y + r.height}; }
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
std::string local(std::string text) { return localizedText(std::move(text)); }
void        renderMenuFrame(UIRenderContext& context) {
    if (screens.empty()) return;
    auto client = ll::service::getClientInstance();
    if (!client) return;
    auto    handle = client->getFontHandle();
    Painter painter(context, *handle.mDefaultFont);
    auto    full  = context.getFullClippingRectangle();
    float   width = full._x1 - full._x0, height = full._y1 - full._y0;
    auto&   s = screen();
    
    Page display;
    display.title   = "LeviSchematic / " + local(s.page->title);
    display.message = local(s.page->message);
    display.dismiss = s.page->dismiss;
    display.rows    = s.page->rows;
    for (auto& r : display.rows) {
        r.label = local(r.label);
        for (auto& a : r.actions) a.label = local(a.label);
        for (auto& choice : r.choices) choice = local(choice);
        for (auto& cell : r.columns) cell = local(cell);
    }
    if (current == PageId::HudLayout) {
        frame = {};
        frame.width = width;
        frame.height = height;
        frame.body = {0, 50, width, std::max(0.f, height - 50)};
        painter.fill({0, 0, width, height}, mce::Color{0.f, 0.f, 0.f, .18f});
        if (hudPositionsPending) {
            app::Config::Hud defaults;
            defaults.position = hudDraft.position;
            auto baseline = hook::drawProjectionHuds(context, defaults, true, -1, false);
            if (baseline.safe.width > 0 && baseline.safe.height > 0) {
                for (size_t i = 0; i < hudDraft.panels.size(); ++i)
                    if (hudDraft.panels[i].x < 0) {
                        auto r = baseline.panels[i];
                        moveHud(hudDraft.panels[i], baseline.safe, r, r.x, r.y);
                    }
                hudPositionsPending = false;
            }
        }
        hudCanvas = hook::drawProjectionHuds(context, hudDraft, true, selectedHud);
        static char const* names[] = {"投影 / Projection", "目标 / Target", "材料 / Materials", "校验 / Verifier"};
        auto add = [&](std::string id, std::string text, UiRect rect, WidgetKind kind, UiOp op) -> Widget& {
            Widget w;
            w.id = id; w.row = id; w.text = std::move(text); w.rect = rect;
            w.clip = {0, 0, width, height}; w.kind = kind; w.op = op;
            frame.widgets.push_back(std::move(w));
            return frame.widgets.back();
        };
        float unit = std::max(1.f, (width - 16) / 6);
        add("hud_panel", local(names[selectedHud]), {8, 8, unit, 20}, WidgetKind::Button, UiOp::Submit);
        auto value = std::to_string(hudDraft.panels[selectedHud].scalePercent);
        for (auto& row : s.page->rows)
            if (row.id == "hud_scale" || row.id == "hud_slider") row.value = value;
        add("hud_scale", value, {8 + unit, 8, unit * .5f, 20}, WidgetKind::Text, UiOp::Submit);
        auto& slider = add("hud_slider", value + "%", {8 + unit * 1.5f, 8, unit * 1.5f, 20}, WidgetKind::Slider, UiOp::Submit);
        slider.value = (hudDraft.panels[selectedHud].scalePercent - 50) / 150.;
        add("hud_reset", local("重置此项 / Reset"), {8 + unit * 3, 8, unit, 20}, WidgetKind::Button, UiOp::Submit);
        add("hud_reset_all", local("全部重置 / Reset all"), {8 + unit * 4, 8, unit, 20}, WidgetKind::Button, UiOp::Submit);
        add("back", local("完成 / Done"), {8 + unit * 5, 8, unit, 20}, WidgetKind::Button, UiOp::Back);
        add("title", local(s.page->message.empty()
                ? "左键拖动面板；大小 50%–200%；退出自动保存 / Drag panels; scale 50%–200%; saved on exit"
                : s.page->message), {8, std::max(34.f, height - 16), width - 16, 12}, WidgetKind::Label, UiOp::None);
    } else frame = layoutPage(display, current, s.view, width, height, [&](std::string const& t) {
        return painter.measure(local(t));
    });
    s.view.scroll = frame.scroll;
    if (current != PageId::HudLayout)
        painter.fill({0, 0, width, height}, mce::Color{0.f, 0.f, 0.f, .69f});
    auto texturePath =
        std::filesystem::absolute(mod().getModDir() / "assets/ui/litematica/gui_widgets.png").generic_string();
    auto texture    = context.getTexture(ResourceLocation(Core::PathView(texturePath), ResourceFileSystem::Raw), false);
    auto loadSprite = [&](char const* name) {
        auto path = std::filesystem::absolute(mod().getModDir() / "assets/ui/minecraft" / name).generic_string();
        return context.getTexture(ResourceLocation(Core::PathView(path), ResourceFileSystem::Raw), false);
    };
    auto normalButton   = loadSprite("button.png");
    auto hoverButton    = loadSprite("button_highlighted.png");
    auto disabledButton = loadSprite("button_disabled.png");
    auto maliPath = std::filesystem::absolute(mod().getModDir() / "assets/ui/malilib/gui_widgets.png").generic_string();
    auto maliTexture = context.getTexture(ResourceLocation(Core::PathView(maliPath), ResourceFileSystem::Raw), false);
    for (auto& w : frame.widgets) {
        auto visible = intersect(w.rect, w.clip);
        if (visible.width <= 0 || visible.height <= 0) continue;
        painter.clip = visible;
        context.setClippingRectangle(area(intersect(w.clip, {0, 0, width, height})));
        context.enableScissorTest(area(intersect(w.clip, {0, 0, width, height})));
        bool hover         = w.rect.contains(mouseX, mouseY) && w.clip.contains(mouseX, mouseY);
        w.hovered          = hover;
        bool        active = focused && focused->id == w.id;
        auto        r      = w.rect;
        std::string caption =
            w.kind == WidgetKind::Text || (w.id == "title" && !display.dismiss) ? w.text : local(w.text);
        if (w.op == UiOp::Back)
            caption = history.empty() ? (uiChinese() ? "关闭" : "Close") : (uiChinese() ? "返回" : "Back");
        if (w.op == UiOp::Search && !active && caption.empty()) caption = uiChinese() ? "搜索…" : "Search…";
        if (w.op == UiOp::Category) {
            static char const* zh[] = {"通用", "信息叠加", "视觉", "颜色", "快捷键", "渲染层"};
            if (uiChinese()) caption = zh[w.index];
        }
        if (w.kind == WidgetKind::Panel) {
            painter.fill(r, mce::Color{.04f, .04f, .04f, .97f});
            painter.outline(r, mce::Color{.6f, .6f, .6f, 1.f});
        } else if (w.kind == WidgetKind::Button) {
            painter.button(
                !w.enabled            ? disabledButton
                : hover || w.selected ? hoverButton
                                      : normalButton,
                r,
                w.enabled ? 3 : 1
            );
            w.texture = !w.enabled          ? "minecraft/button_disabled.png"
                      : hover || w.selected ? "minecraft/button_highlighted.png"
                                            : "minecraft/button.png";
        } else if (w.kind == WidgetKind::Text) {
            painter.fill(r, mce::Color{0.f, 0.f, 0.f, .8f});
            painter.outline(r, active ? mce::Color{1.f, 1.f, 1.f, 1.f} : mce::Color{.5f, .5f, .5f, 1.f});
            if (active) caption = edit.text;
        } else if (w.kind == WidgetKind::Slider) {
            painter.fill(r, mce::Color{0.f, 0.f, 0.f, .8f});
            painter.outline(r, mce::Color{.5f, .5f, .5f, 1.f});
            painter.fill(
                {r.x + static_cast<float>(w.value) * (r.width - 6), r.y + 1, 6, r.height - 2},
                mce::Color{.65f, .65f, .65f, 1.f}
            );
        } else if (w.kind == WidgetKind::Scrollbar) {
            painter.fill(r, mce::Color{.65f, .65f, .65f, 1.f});
            continue;
        } else if (w.kind == WidgetKind::ListRow) {
            painter.fill(r, hover || w.selected ? mce::Color{.25f, .3f, .37f, .9f} : mce::Color{.05f, .05f, .05f, .6f});
            if (hover || w.selected) painter.outline(r, mce::Color{.6f, .6f, .6f, 1.f});
        }
        if (w.icon.width > 0) {
            if (!w.enabled) w.icon.x -= 14;
            bool mali = w.op == UiOp::Search;
            painter.sprite(
                mali ? maliTexture : texture,
                {r.x + 3, r.y + (r.height - w.icon.height) / 2, w.icon.width, w.icon.height},
                w.icon,
                256,
                256
            );
        }
        auto textRect   = r;
        bool center     = w.kind == WidgetKind::Button || w.kind == WidgetKind::Slider;
        textRect.x     += 4;
        textRect.width  = std::max(0.f, textRect.width - 8);
        if (w.icon.width > 0) {
            textRect.x     += w.icon.width + 2;
            textRect.width -= w.icon.width + 2;
        }
        if (w.kind != WidgetKind::Label) {
            textRect.y      += (r.height - 8) / 2;
            textRect.height  = 12;
        }
        context.setClippingRectangle(area(intersect(visible, textRect)));
        painter.clip = intersect(visible, textRect);
        context.enableScissorTest(area(intersect(visible, textRect)));
        w.textBounds = {textRect.x, textRect.y, painter.measure(caption), textRect.height};
        if (active) {
            auto  first = std::min(edit.caret, edit.anchor), last = std::max(edit.caret, edit.anchor);
            float caretX  = painter.measure(edit.text.substr(0, edit.caret));
            float offset  = std::max(0.f, caretX - textRect.width + 3);
            textRect.x   -= offset;
            painter.fill(
                {textRect.x + painter.measure(edit.text.substr(0, first)),
                 r.y + 2,
                 painter.measure(edit.text.substr(first, last - first)),
                 r.height - 4},
                mce::Color{.2f, .35f, .65f, .7f}
            );
            painter.text(
                caption,
                {textRect.x, textRect.y, std::max(textRect.width + offset, painter.measure(caption) + 2), 12}
            );
            painter.fill({textRect.x + caretX, r.y + 3, 1, r.height - 6}, mce::Color{1.f, 1.f, 1.f, 1.f});
        } else
            painter.text(
                caption,
                textRect,
                center,
                w.enabled ? mce::Color{1.f, 1.f, 1.f, 1.f} : mce::Color{.62f, .62f, .62f, 1.f}
            );
        w.text = caption;
    }
    context.setFullClippingRectangle();
    context.disableScissorTest();
    auto json = frameJson(frame, current, ++frameNumber, client->getOptions().getLanguage(), 1 / inverseScale.load());
    if (current == PageId::HudLayout) {
        json["hudPanels"] = nlohmann::ordered_json::array();
        for (size_t i = 0; i < hudCanvas.panels.size(); ++i) {
            auto r = hudCanvas.panels[i];
            json["hudPanels"].push_back({{"id", i}, {"rect", {r.x, r.y, r.width, r.height}},
                {"x", hudDraft.panels[i].x}, {"y", hudDraft.panels[i].y},
                {"scalePercent", hudDraft.panels[i].scalePercent}});
        }
    }
    {
        std::lock_guard lock(mutex);
        lastFrame = std::move(json);
    }
}



LL_TYPE_INSTANCE_HOOK(
    MenuRenderHook,
    ll::memory::HookPriority::Highest,
    ScreenView,
    &ScreenView::render,
    void,
    UIRenderContext& ctx
) {
    auto owns = [&](std::shared_ptr<AbstractScene> const& value) {
        return value && this == static_cast<UIScene*>(value.get())->mScreenView.get();
    };
    bool exiting = owns(retired);
    for (auto const& value : exitingScenes) {
        if (owns(value.lock())) {
            exiting = true;
            break;
        }
    }
    if (!enabled || (!owns(scene) && !exiting))
        return origin(ctx);
    
    
    if (auto const& tree = mVisualTree.get(); tree && tree->mRootControl) {
        
        
        tree->mRootControl->mVisible = ::ui::VisibilityFlag::None;
    }
    origin(ctx);
    if (popping || exiting) return;
    try {
        renderMenuFrame(ctx);
    } catch (std::exception const& e) {
        report(std::string("render failed: ") + e.what());
    }
}
LL_TYPE_INSTANCE_HOOK(
    MenuExitHook,
    ll::memory::HookPriority::Normal,
    UIScene,
    &UIScene::$onScreenExit,
    void,
    bool                           isPopping,
    bool                           transitions,
    std::shared_ptr<AbstractScene> pushed
) {
    bool mine = scene.get() == this;
    if (mine && isPopping && !popping && !screens.empty()) (void)savePage();
    origin(isPopping, mine ? false : transitions, std::move(pushed));
    if (mine && isPopping) {
        capturesInput = false;
        opened        = false;
        exitingScenes.emplace_back(scene);
        retired       = std::move(scene);
        clearPages();
        seen = popping = false;
        std::lock_guard lock(mutex);
        requests.didClose();
    }
}
LL_TYPE_INSTANCE_HOOK(MenuBehindHook, ll::memory::HookPriority::Normal, UIScene, &UIScene::$renderGameBehind, bool) {
    if (scene.get() == this) return true;
    return origin();
}
LL_TYPE_INSTANCE_HOOK(MenuHurtHook, ll::memory::HookPriority::Normal, UIScene, &UIScene::$closeOnPlayerHurt, bool) {
    if (scene.get() == this) return false;
    return origin();
}

LL_STATIC_HOOK(
    MenuTextHook,
    ll::memory::HookPriority::High,
    &ClientInputCallbacks::handleTextChar,
    void,
    IClientInstance&   client,
    std::string const& text,
    FocusImpact        impact
) {
    if (capturesInput && capturesText) {
        enqueue({Input::Text, 0, 0, 0, 0, text});
        return;
    }
    origin(client, text, impact);
}
LL_TYPE_INSTANCE_HOOK(
    MenuFocusHook,
    ll::memory::HookPriority::Normal,
    MinecraftGame,
    &MinecraftGame::$onAppFocusLost,
    void
) {
    origin();
    if (opened) enqueue({Input::FocusLost});
}
} 

void start() {
    if (enabled.exchange(true)) return;
    reloadTranslations();
    MenuRenderHook::hook();
    MenuExitHook::hook();
    MenuBehindHook::hook();
    MenuHurtHook::hook();
    MenuTextHook::hook();
    MenuFocusHook::hook();
    auto& bus        = ll::event::EventBus::getInstance();
    keyboardListener = bus.emplaceListener<ll::event::KeyInputEvent>(
        [](auto& e) {
            int code = e.keyCode();
            if (code < 0 || code >= 256) return;
            bool down = e.isDown();
            if (code == VK_F7 && down && !opened && canOpen) {
                requestOpen();
                heldKeys[code] = true;
                e.cancel();
                return;
            }
            bool consumed = down ? capturesInput.load() : heldKeys[code].exchange(false);
            if (!consumed) return;
            if (down) heldKeys[code] = true;
            e.cancel();
            enqueue({Input::Key, code, down});
        },
        ll::event::EventPriority::High
    );
    mouseListener = bus.emplaceListener<ll::event::MouseInputEvent>(
        [](auto& e) {
            int code = e.actionButtonId();
            if (code < 0 || code >= 8 || code == 7) return;
            bool consumed = capturesInput;
            if (code >= 1 && code <= 3) {
                bool down = e.buttonData() != 0;
                if (mouseDown[code].exchange(down) == down) {
                    if (heldMouse[code]) e.cancel();
                    return;
                }
                consumed = down ? capturesInput.load() : heldMouse[code].exchange(false);
                if (down && consumed) heldMouse[code] = true;
            }
            if (!consumed) return;
            
            if (code != 0) e.cancel();
            enqueue({Input::Mouse, code, e.buttonData(), e.x() * inverseScale.load(), e.y() * inverseScale.load()});
        },
        ll::event::EventPriority::High
    );
    report("C++ menu backend installed");
}
void stop() {
    if (!enabled.exchange(false)) return;
    if (!screens.empty()) (void)savePage();
    closeMenu(false);
    clearPages();
    capturesInput = opened = canOpen = false;
    auto& bus                        = ll::event::EventBus::getInstance();
    if (keyboardListener) bus.removeListener(keyboardListener);
    if (mouseListener) bus.removeListener(mouseListener);
    keyboardListener.reset();
    mouseListener.reset();
    MenuFocusHook::unhook();
    MenuTextHook::unhook();
    MenuHurtHook::unhook();
    MenuBehindHook::unhook();
    MenuExitHook::unhook();
    MenuRenderHook::unhook();
    retired.reset();
    scene.reset();
    exitingScenes.clear();
    std::lock_guard lock(mutex);
    requests.clear();
    inputs.clear();
}
bool isOpen() { return opened; }
void requestOpen(PageId id) {
    std::lock_guard lock(mutex);
    if (enabled) requests.open(id);
}
void requestToggle() {
    std::lock_guard lock(mutex);
    if (enabled) requests.toggle(opened);
}
std::string diagnostics() {
    std::lock_guard lock(mutex);
    return "backend=cpp enabled=" + std::to_string(enabled.load()) + " opened=" + std::to_string(opened.load())
         + " page=" + std::to_string(pageNumber.load()) + " frames=" + std::to_string(frameNumber.load())
         + " pendingInput=" + std::to_string(inputs.size()) + " last=" + lastEvent;
}
std::string exportLayout() {
    nlohmann::ordered_json json;
    {
        std::lock_guard lock(mutex);
        json = lastFrame;
    }
    if (json.is_null()) return "No completed UI frame is available.";
    try {
        auto dir = mod().getDataDir() / "diagnostics";
        std::filesystem::create_directories(dir);
        auto          file = dir / "ui-layout.json";
        std::ofstream out(file, std::ios::binary);
        out << json.dump(2) << '\n';
        if (!out) return "Cannot write UI layout.";
        return file.string();
    } catch (std::exception const& e) {
        return e.what();
    }
}
void pump() {
    retired.reset();
    std::erase_if(exitingScenes, [](auto const& value) { return value.expired(); });
    if (!enabled) return;
    auto client = ll::service::getClientInstance();
    if (!client) return;
    synchronizeBindings(*client);
    auto session = app::accessSession();
    canOpen      = session && client->isInWorldAndNotShowingAnyMenuScreens();
    if (!session) {
        if (scene) closeMenu(false);
        return;
    }
    inverseScale = client->getGuiData()->mInvGuiScale;
    if (scene
        && (sessionGeneration != session->sessionGeneration() || resourceGeneration != session->resourceGeneration())) {
        closeMenu(false);
        return;
    }
    if (scene) {
        bool top = client->getClientSceneStack()->getTopScene() == scene.get();
        if (top) {
            seen        = true;
            unseenTicks = 0;
        } else if (seen || ++unseenTicks > 180) {
            capturesInput = false;
            releaseKeyboard();
            if (!seen && unseenTicks > 180) {
                retired = std::move(scene);
                clearPages();
                opened = false;
            }
            return;
        }
        bool active = top && !popping;
        if (capturesInput && !active) input({Input::FocusLost, 0, 0, 0, 0, {}, inputGeneration.load()});
        capturesInput = top;
    }
    std::optional<PageId> wanted;
    bool                  close = false;
    std::deque<Input>     work;
    {
        std::lock_guard lock(mutex);
        wanted = std::exchange(requests.page, {});
        close  = std::exchange(requests.close, false);
        work.swap(inputs);
    }
    if (close) {
        closeMenu();
        return;
    }
    if (wanted && popping) {
        std::lock_guard lock(mutex);
        requests.open(*wanted);
    }
    if (wanted && !popping) {
        if (!scene) {
            reloadTranslations();
            clearPages();
            sessionGeneration  = session->sessionGeneration();
            resourceGeneration = session->resourceGeneration();
            navigate(*wanted, false);
            scene = client->getSceneFactory().createCommonDialogInfoScreen("LeviSchematic", "");
            if (!scene) {
                report("scene creation failed");
                clearPages();
                return;
            }
            seen = popping = false;
            unseenTicks    = 0;
            opened         = true;
            client->getClientSceneStack()->pushScreen(scene, false);
            report("menu scene queued");
        } else if (*wanted == current) {
            if (savePage()) {
                auto& p = screen().page;
                clearPage(*p);
                p     = buildPage(current);
                frame = {};
                ++inputGeneration;
            }
        } else navigate(*wanted);
    }
    if (!scene || popping) return;
    for (auto const& e : work) {
        try {
            input(e);
        } catch (std::exception const& e) {
            screen().page->message = e.what();
        }
    }
    if (!screens.empty()) {
        auto fn = screen().page->refresh;
        if (fn) fn();
    }
}
} 
