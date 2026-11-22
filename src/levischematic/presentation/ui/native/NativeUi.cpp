#include "Clipboard.h"
#include "Painter.h"
#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/app/lifecycle/AppKernel.h"
#include "levischematic/presentation/hud/runtime/ProjectionHudHook.h"
#include "levischematic/presentation/ui/bindings/Bindings.h"
#include "levischematic/presentation/ui/i18n/UiI18n.h"
#include "levischematic/presentation/ui/layout/UiLayout.h"
#include "levischematic/presentation/ui/model/Ui.h"
#include "levischematic/presentation/ui/model/UiTextEdit.h"
#include "levischematic/presentation/ui/native/OpenRequests.h"
#include "levischematic/shared/i18n/I18n.h"
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
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/UIRenderContext.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/SceneFactory.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"
#include "mc/client/input/KeyboardManager.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/common/client/renderer/helpers/MeshHelpers.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTextureData.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/registry/ItemRegistryManager.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
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
bool     seen = false, popping = false, keyboardClaimed = false, keyboardEnabled = false;
PageId   current           = PageId::MainMenu;
uint64_t sessionGeneration = 0, resourceGeneration = 0;
struct Screen {
    std::shared_ptr<Page> page;
    UiViewState           view;
};
std::map<PageId, Screen> screens;
std::vector<PageId>      history;
UiFrame                  frame;
std::optional<Widget>    focused, dragging, pressed, pressedRight;
UiTextEdit               edit;
bool                     shift = false, control = false;
float                    mouseX = -1, mouseY = -1;
unsigned                 unseenTicks = 0;
app::Config::Hud         hudDraft;
HudCanvas                hudCanvas;
int                      selectedHud = 0;
bool                     hudDirty = false, hudDragging = false;
bool                     hudPositionsPending = false;
float                    hudGrabX = 0, hudGrabY = 0;

Modifiers readModifiers() {
    return {
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0,
        (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0,
        (GetAsyncKeyState(VK_MENU) & 0x8000) != 0
    };
}
void setHudScale(std::string text) {
    try {
        size_t end   = 0;
        double value = std::stod(text, &end);
        if (end != text.size() || !std::isfinite(value) || value < 50 || value > 200 || value != std::round(value))
            throw std::out_of_range("scale");
        hudDraft.panels[selectedHud].scalePercent = static_cast<int>(value);
        hudDirty                                  = true;
        if (!screens.empty()) screens.at(current).page->message.clear();
    } catch (...) {
        if (!screens.empty()) screens.at(current).page->message = tr("ui.native.scale_50_200");
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
        screen().view.search     = text;
        screen().view.searchOpen = !text.empty();
        screen().view.scroll     = 0;
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
        value.hud  = hudDraft;
        auto error = app::configStore().submit(value);
        if (error.empty()) error = app::configStore().save();
        if (!error.empty()) {
            screen().page->message = error;
            return false;
        }
        hudDirty = false;
    }
    auto p = screen().page;
    dismissModal(p);
    return !p->save || p->save();
}
void navigate(PageId id, bool push = true) {
    if (!screens.empty() && !savePage()) return;
    if (id == PageId::MainMenu) history.clear();
    else if (auto it = std::find(history.begin(), history.end(), id); it != history.end()) {
        history.erase(it, history.end());
        push = false;
    }
    if (!screens.empty() && push && id != current) history.push_back(current);
    current    = id;
    pageNumber = static_cast<int>(id);
    if (!screens.contains(id)) screens.emplace(id, Screen{buildPage(id), {}});
    else if (
        id == PageId::Projection || id == PageId::Placement || id == PageId::Resources || id == PageId::Tasks
        || id == PageId::Settings || id == PageId::MainMenu || id == PageId::Load || id == PageId::SchematicManager
        || id == PageId::AreaSelections || id == PageId::Selection || id == PageId::SubRegion
        || id == PageId::PlacementGrid || id == PageId::Verifier
    ) {
        auto& page = screens.at(id).page;
        clearPage(*page);
        page = buildPage(id);
    }
    if (id == PageId::HudLayout) {
        hudDraft = app::configStore().editableSnapshot().hud;
        hudDirty = hudDragging = false;
        hudPositionsPending    = true;
        selectedHud            = 0;
        auto p                 = screens.at(id).page;
        p->rows.clear();
        PageBuilder b{p};
        b.button("hud_panel", tr("ui.native.panel"), [] { selectedHud = (selectedHud + 1) % 4; })
            .input("hud_scale", tr("ui.native.scale"), "100", setHudScale)
            .input("hud_slider", tr("ui.native.scale_2"), "100", setHudScale)
            .button(
                "hud_reset",
                tr("ui.native.reset"),
                [] {
                    hudDraft.panels[selectedHud] = {};
                    hudDirty = hudPositionsPending = true;
                }
            )
            .button("hud_reset_all", tr("ui.native.reset_all"), [] {
                hudDraft = {};
                hudDirty = hudPositionsPending = true;
            });
        p->rows[2].kind    = RowKind::Slider;
        p->rows[2].minimum = 50;
        p->rows[2].maximum = 200;
        p->rows[2].step    = 1;
    }
    frame = {};
    pressed.reset();
    pressedRight.reset();
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
        auto clear   = std::exchange(p->captureEscape, {});
        p->capturing = false;
        p->rawKey    = {};
        p->message.clear();
        if (clear) clear();
        return;
    }
    if (p->dismiss) {
        dismissModal(p);
        frame = {};
        return;
    }
    if (focused) {
        commitEdit();
        if (current != PageId::Settings) return;
    }
    if (auto parentPage = configurationParent(current)) {
        auto parent = *parentPage;
        if (!savePage()) return;
        history.clear();
        if (parent == PageId::Settings) history.push_back(PageId::MainMenu);
        navigate(parent, false);
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
void invoke(Widget w, float x = 0, bool secondary = false) {
    if (screens.empty()) return;
    if (w.op == UiOp::Back) {
        back();
        return;
    }
    if (w.op == UiOp::Category) {
        if (!savePage()) return;
        if (w.index == 5)
            navigate(PageId::KeyBindings, current == PageId::Settings || current == PageId::Layers ? false : true);
        else if (w.index == 6)
            navigate(PageId::Layers, current == PageId::Settings || current == PageId::KeyBindings ? false : true);
        else {
            navigate(PageId::Settings, current == PageId::KeyBindings || current == PageId::Layers ? false : true);
            screen().view.category = w.index;
            screen().view.scroll   = 0;
        }
        return;
    }
    if (w.op == UiOp::SearchToggle) {
        auto& view = screen().view;
        if (view.searchOpen || !view.search.empty()) {
            commitEdit();
            view.searchOpen = false;
            view.search.clear();
            view.scroll = 0;
        } else {
            view.searchOpen = true;
            Widget field;
            field.id   = "search";
            field.op   = UiOp::Search;
            field.kind = WidgetKind::Text;
            field.rect = w.rect;
            focus(field);
        }
        return;
    }
    if (w.op == UiOp::Sort) {
        commitEdit();
        auto fn = screen().page->sort;
        if (fn) fn(w.index);
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

    if (w.op == UiOp::Adjust) {
        auto fn = r->adjust;
        if (fn) fn((secondary ? -1 : 1) * adjustAmount(r->adjustMode, currentModifiers()));
        return;
    }
    if (secondary && r->secondary) {
        auto fn = r->secondary;
        fn();
        return;
    }
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
            int n = static_cast<int>(std::max<size_t>(1, r->choices.size()));
            value = std::to_string((i + (secondary ? n - 1 : 1)) % n);
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
            moveHud(
                hudDraft.panels[selectedHud],
                hudCanvas.safe,
                hudCanvas.panels[selectedHud],
                e.x - hudGrabX,
                e.y - hudGrabY
            );
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
                hudGrabX    = e.x - r.x;
                hudGrabY    = e.y - r.y;
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
    if (e.code == 2) {
        if (e.data) {
            if (auto w = frame.hit(e.x, e.y)) pressedRight = *w;
        } else {
            if (pressedRight)
                if (auto w = frame.hit(e.x, e.y); w && w->id == pressedRight->id) invoke(*w, e.x, true);
            pressedRight.reset();
        }
        return;
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

mce::Color argb(uint32_t c) {
    return {((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f, (c & 255) / 255.f, ((c >> 24) & 255) / 255.f};
}
std::string pageName(PageId id) {
    auto it = screens.find(id);
    if (it == screens.end()) return {};
    auto const& p = *it->second.page;
    return p.crumb.empty() ? p.title : p.crumb;
}
std::string breadcrumb() {
    auto const& page = *screen().page;
    if (page.fullTitle) return page.title;
    std::string text = "LeviSchematic";
    if (auto const& v = mod().getManifest().version; v && v->to_string() != "0.0.0") text += " v" + v->to_string();
    bool first = true;
    auto push  = [&](std::string const& name) {
        if (name.empty()) return;
        text  += (first ? " " : " => ") + name;
        first  = false;
    };
    for (auto id : history) push(pageName(id));
    push(pageName(current));
    return text;
}
void drawItemIcon(UIRenderContext& context, std::string const& name, int aux, UiRect r) {
    static std::map<std::pair<std::string, int>, std::shared_ptr<ItemStack>> stacks;
    auto&                                                                    stack = stacks[{name, aux}];
    if (!stack || !*stack) {
        stack     = std::make_shared<ItemStack>();
        auto item = ItemRegistryManager::getItemRegistry().lookupByNameNoAlias(std::string_view(name));
        if (item) stack->reinit(*item, 1, aux);
    }
    if (!*stack) return;
    auto client = ll::service::getClientInstance();
    if (!client) return;
    auto&                  ui            = static_cast<MinecraftUIRenderContext&>(context);
    ScreenContext&         screenContext = ui.mScreenContext;
    BaseActorRenderContext actors(screenContext, *client, client->getMinecraftGame_DEPRECATED());
    ItemRenderer&          renderer = actors.mItemRenderer;
    renderer.renderGuiItemNew(actors, *stack, 0, r.x, r.y, false, 1.f, 1.f, r.width / 16.f, 0);
    Tessellator& tessellator = screenContext.tessellator;
    if (!tessellator.mTessellating) return;
    static mce::MaterialPtr const material(mce::RenderMaterialGroup::common(), HashedString("ui_item"));
    std::variant<std::monostate, mce::TexturePtr, mce::ClientTexture, mce::ServerTexture> texture{
        std::in_place_type<mce::TexturePtr>,
        renderer.mAtlasTexture.get()
    };
    MeshHelpers::renderMeshImmediately(screenContext, tessellator, material, texture, OffscreenCaptureDescription{});
}
void renderMenuFrame(UIRenderContext& context) {
    if (screens.empty()) return;
    auto client = ll::service::getClientInstance();
    if (!client) return;
    auto    handle = client->getFontHandle();
    Painter painter(context, *handle.mDefaultFont);
    auto    full  = context.getFullClippingRectangle();
    float   width = full._x1 - full._x0, height = full._y1 - full._y0;
    auto&   s = screen();

    Page display;
    display.title          = s.page->dismiss ? s.page->title : breadcrumb();
    display.message        = s.page->message;
    display.dismiss        = s.page->dismiss;
    display.rows           = s.page->rows;
    display.layout         = s.page->layout;
    display.path           = s.page->path;
    display.info           = s.page->info;
    display.sidePanel      = s.page->sidePanel;
    display.header         = s.page->header;
    display.sortColumn     = s.page->sortColumn;
    display.sortDescending = s.page->sortDescending;
    display.sort           = s.page->sort;
    display.listX          = s.page->listX;
    display.listY          = s.page->listY;
    display.listWidth      = s.page->listWidth;
    display.listHeight     = s.page->listHeight;
    display.searchable     = s.page->searchable;
    auto localize          = [](Row& r) {
        r.label   = r.label;
        r.tooltip = r.tooltip;
        for (auto& choice : r.choices) choice = choice;
        for (auto& cell : r.columns) cell = cell;
    };
    for (auto& r : display.rows) {
        localize(r);
        for (auto& a : r.actions) localize(a);
    }
    for (auto& line : display.info) {
        line.label = line.label;
        line.value = line.value;
    }
    for (auto& h : display.header) h = h;
    if (current == PageId::HudLayout) {
        frame        = {};
        frame.width  = width;
        frame.height = height;
        frame.body   = {0, 50, width, std::max(0.f, height - 50)};
        painter.fill({0, 0, width, height}, mce::Color{0.f, 0.f, 0.f, .18f});
        if (hudPositionsPending) {
            app::Config::Hud defaults;
            defaults.position = hudDraft.position;
            auto baseline     = hook::drawProjectionHuds(context, defaults, true, -1, false);
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
        static char const* names[] =
            {"hud.panel.projection", "hud.panel.target", "hud.panel.materials", "hud.panel.verifier"};
        auto add = [&](std::string id, std::string text, UiRect rect, WidgetKind kind, UiOp op) -> Widget& {
            Widget w;
            w.id       = id;
            w.row      = id;
            w.text     = std::move(text);
            w.rect     = rect;
            w.clip     = {0, 0, width, height};
            w.kind     = kind;
            w.op       = op;
            w.centered = kind == WidgetKind::Button || kind == WidgetKind::Slider;
            frame.widgets.push_back(std::move(w));
            return frame.widgets.back();
        };
        float unit = std::max(1.f, (width - 16) / 6);
        add("hud_panel", tr(names[selectedHud]), {8, 8, unit, 20}, WidgetKind::Button, UiOp::Submit);
        auto value = std::to_string(hudDraft.panels[selectedHud].scalePercent);
        for (auto& row : s.page->rows)
            if (row.id == "hud_scale" || row.id == "hud_slider") row.value = value;
        add("hud_scale", value, {8 + unit, 8, unit * .5f, 20}, WidgetKind::Text, UiOp::Submit);
        auto& slider =
            add("hud_slider", value + "%", {8 + unit * 1.5f, 8, unit * 1.5f, 20}, WidgetKind::Slider, UiOp::Submit);
        slider.value = (hudDraft.panels[selectedHud].scalePercent - 50) / 150.;
        add("hud_reset", tr("ui.native.reset"), {8 + unit * 3, 8, unit, 20}, WidgetKind::Button, UiOp::Submit);
        add("hud_reset_all", tr("ui.native.reset_all"), {8 + unit * 4, 8, unit, 20}, WidgetKind::Button, UiOp::Submit);
        add("back", tr("ui.native.done"), {8 + unit * 5, 8, unit, 20}, WidgetKind::Button, UiOp::Back);
        add("title",
            s.page->message.empty() ? tr("ui.native.drag_panels_scale_50_200_saved_on") : s.page->message,
            {8, std::max(34.f, height - 16), width - 16, 12},
            WidgetKind::Label,
            UiOp::None);
    } else
        frame = layoutPage(display, current, s.view, width, height, [&](std::string const& t) {
            return painter.measure(t);
        });
    s.view.scroll = frame.scroll;
    if (current != PageId::HudLayout) painter.fill({0, 0, width, height}, mce::Color{0.f, 0.f, 0.f, .55f});
    auto rawTexture = [&](char const* relative) {
        auto path = std::filesystem::absolute(mod().getModDir() / relative).generic_string();
        return context.getTexture(ResourceLocation(Core::PathView(path), ResourceFileSystem::Raw), false);
    };
    auto          texture        = rawTexture("assets/ui/litematica/gui_widgets.png");
    auto          maliTexture    = rawTexture("assets/ui/malilib/gui_widgets.png");
    auto          normalButton   = rawTexture("assets/ui/minecraft/button.png");
    auto          hoverButton    = rawTexture("assets/ui/minecraft/button_highlighted.png");
    auto          disabledButton = rawTexture("assets/ui/minecraft/button_disabled.png");
    Widget const* tip            = nullptr;
    for (auto& w : frame.widgets) {
        auto visible = intersect(w.rect, w.clip);
        if (visible.width <= 0 || visible.height <= 0) continue;
        painter.clip = visible;
        context.setClippingRectangle(area(intersect(w.clip, {0, 0, width, height})));
        context.enableScissorTest(area(intersect(w.clip, {0, 0, width, height})));
        bool hover = w.rect.contains(mouseX, mouseY) && w.clip.contains(mouseX, mouseY);
        w.hovered  = hover;
        if (hover && !w.tooltip.empty()) tip = &w;
        bool        active  = focused && focused->id == w.id;
        auto        r       = w.rect;
        std::string caption = w.text;
        if (w.op == UiOp::Back) caption = history.empty() ? tr("ui.native.close") : tr("ui.native.back");
        if (w.op == UiOp::Search && !active && caption.empty()) caption = tr("ui.native.search");
        mce::Color textColor = w.color ? argb(w.color) : mce::Color{1.f, 1.f, 1.f, 1.f};
        if (!w.enabled && w.kind != WidgetKind::Label) textColor = mce::Color{.62f, .62f, .62f, 1.f};
        bool drawIcon = w.icon.width > 0;
        switch (w.kind) {
        case WidgetKind::Panel:
            painter.fill(r, mce::Color{0.f, 0.f, 0.f, .62f});
            painter.outline(r, mce::Color{.75f, .75f, .75f, 1.f});
            break;
        case WidgetKind::Button:
            painter.button(!w.enabled ? disabledButton : hover ? hoverButton : normalButton, r, w.enabled ? 3 : 1);
            w.texture = !w.enabled ? "minecraft/button_disabled.png"
                      : hover      ? "minecraft/button_highlighted.png"
                                   : "minecraft/button.png";
            if (!w.enabled && w.op == UiOp::Category) textColor = mce::Color{1.f, 1.f, .63f, 1.f};
            break;
        case WidgetKind::Text:
            painter.fill(r, mce::Color{0.f, 0.f, 0.f, 1.f});
            painter.outline(r, active ? mce::Color{1.f, 1.f, 1.f, 1.f} : mce::Color{.63f, .63f, .63f, 1.f});
            if (active) caption = edit.text;
            else if (w.op == UiOp::Search && w.text.empty()) textColor = mce::Color{.5f, .5f, .5f, 1.f};
            break;
        case WidgetKind::Slider:
            painter.fill(r, mce::Color{0.f, 0.f, 0.f, .8f});
            painter.outline(r, mce::Color{.5f, .5f, .5f, 1.f});
            painter.fill(
                {r.x + static_cast<float>(w.value) * (r.width - 6), r.y + 1, 6, r.height - 2},
                mce::Color{.65f, .65f, .65f, 1.f}
            );
            break;
        case WidgetKind::ItemIcon:
            if (visible.width >= r.width && visible.height >= r.height) {
                try {
                    drawItemIcon(context, w.texture, w.index, r);
                } catch (std::exception const& e) {
                    report(std::string("item icon failed: ") + e.what());
                }
            }
            continue;
        case WidgetKind::Swatch: {
            auto c = argb(w.color);
            c.a    = 1;
            painter.fill(r, mce::Color{1.f, 1.f, 1.f, 1.f});
            painter.fill({r.x + 1, r.y + 1, r.width - 2, r.height - 2}, mce::Color{0.f, 0.f, 0.f, 1.f});
            painter.fill({r.x + 2, r.y + 2, r.width - 4, r.height - 4}, c);
            continue;
        }
        case WidgetKind::Scrollbar:
            painter.clip = {0, 0, width, height};
            painter.fill({r.x, frame.body.y, r.width, frame.body.height}, mce::Color{0.f, 0.f, 0.f, .5f});
            painter.fill(r, mce::Color{.75f, .75f, .75f, 1.f});
            continue;
        case WidgetKind::ListRow:
            painter.fill(
                r,
                w.selected ? mce::Color{.5f, .5f, .5f, .55f}
                : hover    ? mce::Color{.4f, .4f, .4f, .45f}
                : w.index  ? mce::Color{.25f, .25f, .25f, .35f}
                           : mce::Color{.15f, .15f, .15f, .35f}
            );
            if (w.selected) painter.outline(r, mce::Color{.9f, .9f, .9f, 1.f});
            continue;
        case WidgetKind::HeaderCell:
            painter.fill(
                r,
                hover && w.op != UiOp::None ? mce::Color{.3f, .3f, .3f, .9f} : mce::Color{.12f, .12f, .12f, .9f}
            );
            painter.outline(r, mce::Color{.45f, .45f, .45f, 1.f});
            break;
        case WidgetKind::Checkbox:
            painter.sprite(texture, {r.x, r.y, w.icon.width, w.icon.height}, w.icon, 256, 256);
            drawIcon = false;
            break;
        case WidgetKind::IconButton:
            if (!w.parts.empty()) {
                painter.fill(r, mce::Color{0.f, 0.f, 0.f, 1.f});
                painter.outline(r, hover ? mce::Color{1.f, 1.f, 1.f, 1.f} : mce::Color{.75f, .75f, .75f, 1.f});
                for (auto const& part : w.parts)
                    painter.sprite(
                        maliTexture,
                        {r.x + 1 + part.dx, r.y + 1 + part.dy, part.w, part.h},
                        {part.u, part.v, part.w, part.h},
                        256,
                        256
                    );
                break;
            }
            if (w.op != UiOp::None && hover && !w.variants) painter.outline(r, mce::Color{1.f, 1.f, 1.f, .8f});
            break;
        default:
            break;
        }
        if (drawIcon) {
            auto uv = w.icon;
            if (w.variants) uv.x += uv.width * (!w.enabled ? 0 : hover ? 2 : 1);
            else if (!w.enabled && w.kind == WidgetKind::Button && w.atlas == 1 && uv.x == 116) uv.x -= 14;
            UiRect at{r.x + 3, r.y + (r.height - uv.height) / 2, uv.width, uv.height};
            if (w.kind == WidgetKind::IconButton)
                at = {r.x + (r.width - uv.width) / 2, r.y + (r.height - uv.height) / 2, uv.width, uv.height};
            if (w.kind == WidgetKind::HeaderCell)
                at = {r.x + r.width - uv.width - 1, r.y + (r.height - uv.height) / 2, uv.width, uv.height};
            painter.sprite(w.atlas == 2 ? maliTexture : texture, at, uv, 256, 256);
        }
        auto suffix = w.suffix;
        if (w.kind == WidgetKind::IconButton || (caption.empty() && suffix.empty() && !active)) {
            w.text = caption;
            continue;
        }
        bool plain      = w.kind == WidgetKind::Label;
        auto textRect   = r;
        textRect.x     += plain ? 0 : 4;
        textRect.width  = std::max(0.f, textRect.width - (plain ? 0 : 8));
        if (w.kind == WidgetKind::Checkbox) {
            textRect.x     = r.x + 14;
            textRect.width = std::max(0.f, r.width - 14);
        } else if (drawIcon && w.kind != WidgetKind::HeaderCell) {
            textRect.x     += w.icon.width + 4;
            textRect.width -= w.icon.width + 4;
        } else if (w.kind == WidgetKind::HeaderCell && drawIcon) textRect.width -= w.icon.width;
        if (!plain) {
            textRect.y      += (r.height - 8) / 2;
            textRect.height  = 12;
        }
        context.setClippingRectangle(area(intersect(visible, textRect)));
        painter.clip = intersect(visible, textRect);
        context.enableScissorTest(area(intersect(visible, textRect)));
        float total  = painter.measure(caption) + painter.measure(suffix);
        w.textBounds = {textRect.x, textRect.y, total, textRect.height};
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
        } else if (!suffix.empty()) {
            float x  = w.centered ? textRect.x + std::max(0.f, (textRect.width - total) / 2) : textRect.x;
            float cw = painter.measure(caption);
            if (!caption.empty()) painter.text(caption, {x, textRect.y, cw + 1, textRect.height}, false, textColor);
            painter.text(
                suffix,
                {x + cw, textRect.y, std::max(0.f, textRect.x + textRect.width - x - cw), textRect.height},
                false,
                w.enabled ? argb(w.suffixColor) : textColor
            );
        } else painter.text(caption, textRect, w.centered, textColor);
        w.text = caption + suffix;
    }
    if (tip && !display.dismiss && !dragging) {
        auto measure = [&](std::string const& t) { return painter.measure(t); };
        auto lines   = wrapLines(tip->tooltip, std::min(320.f, width * .6f), measure);
        auto box     = tooltipRect(lines, mouseX, mouseY, width, height, measure);
        painter.clip = {0, 0, width, height};
        context.setClippingRectangle(area(painter.clip));
        context.enableScissorTest(area(painter.clip));
        painter.fill(box, mce::Color{.063f, 0.f, .063f, .94f});
        painter.outline(box, mce::Color{.31f, 0.f, 1.f, .4f});
        painter.outline({box.x + 1, box.y + 1, box.width - 2, box.height - 2}, mce::Color{.16f, 0.f, .5f, .4f});
        float y = box.y + 4;
        for (auto const& line : lines) {
            painter.text(line, {box.x + 4, y, box.width - 6, 10});
            y += 10;
        }
    }
    context.setFullClippingRectangle();
    context.disableScissorTest();
    auto json = frameJson(frame, current, ++frameNumber, client->getOptions().getLanguage(), 1 / inverseScale.load());
    if (current == PageId::HudLayout) {
        json["hudPanels"] = nlohmann::ordered_json::array();
        for (size_t i = 0; i < hudCanvas.panels.size(); ++i) {
            auto r = hudCanvas.panels[i];
            json["hudPanels"].push_back({
                {"id",           i                              },
                {"rect",         {r.x, r.y, r.width, r.height}  },
                {"x",            hudDraft.panels[i].x           },
                {"y",            hudDraft.panels[i].y           },
                {"scalePercent", hudDraft.panels[i].scalePercent}
            });
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
    if (!enabled || (!owns(scene) && !exiting)) return origin(ctx);


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
        retired = std::move(scene);
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
} // namespace

Modifiers currentModifiers() { return readModifiers(); }
void      start() {
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
} // namespace levischematic::ui
