#include "Bindings.h"
#include "OpenRequests.h"
#include "Ui.h"
#include "UiI18n.h"
#include "UiLayout.h"
#include "levischematic/LeviSchematic.h"
#include "levischematic/app/AppKernel.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/UIDefRepository.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/UIPropertyBag.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/SceneFactory.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenEvent.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/ToggleChangeEventData.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"
#include "mc/deps/core/string/StringHash.h"
#include "mc/deps/json/Reader.h"
#include "mc/deps/json/Value.h"
#include <atomic>
#include <deque>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>

namespace levischematic::ui {
namespace {
std::atomic_bool     enabled = false;
std::atomic_bool     opened  = false;
std::mutex           requestMutex;
OpenRequests         requests;
std::atomic_uint64_t requestCount{0};
std::atomic_uint64_t consumedCount{0};
std::atomic_uint64_t definitionCount{0};
std::atomic_uint64_t tickCount{0};
std::string          lastEvent   = "not started";
std::string          layoutState = "not observed";

void report(std::string message) {
    {
        std::lock_guard lock(requestMutex);
        lastEvent = message;
    }
    LeviSchematic::getInstance().getSelf().getLogger().info("UI: {}", message);
}



Json::Value const& definition() {
    static Json::Value const value = [] {
        Json::Value result;
        if (!Json::Reader{}.parse(buildNativeDefinitions().dump(), result, false))
            throw std::runtime_error("Invalid UI definition");
        return result;
    }();
    return value;
}
LL_TYPE_INSTANCE_HOOK(
    DefinitionHook,
    ll::memory::HookPriority::Normal,
    UIDefRepository,
    &UIDefRepository::$findDef,
    Json::Value const&,
    std::string const& space,
    std::string const& name
) {
    if (space == "levischematic") {
        
        
        if (origin("common_buttons", "light_text_button").isNull() || origin("common", "screen_background").isNull()
            || origin("common", "dialog_background_opaque").isNull() || origin("common", "text_edit_box").isNull()
            || origin("common", "container_scroll_panel").isNull() || origin("common_tabs", "tab_top").isNull()
            || origin("common", "focus_border_white").isNull()) {
            throw std::runtime_error("Required vanilla UI templates are unavailable");
        }
        ++definitionCount;
        return definition()[name];
    }
    return origin(space, name);
}

class Controller final : public ScreenController {
    PageTabs              tabs;
    std::shared_ptr<Page> page;
    uint64_t              generation;
    uint64_t              resources;

    std::optional<std::string>        editing;
    std::string                       edited;
    std::deque<std::function<void()>> pending;
    bool                              closing        = false;
    bool                              closeScheduled = false;
    bool                              backHandled    = false;
    std::weak_ptr<UIScene>            scene;
    unsigned                          observationTicks = 0;
    void                              back() {
        
        if (std::exchange(backHandled, true)) return;
        if (page->capturing) {
            page->capturing = false;
            return;
        }
        if (editing) {
            editing.reset();
            return;
        }
        bool dirty = false;
        for (auto const& value : tabs.pages)
            if (value && value->dirty) {
                dirty        = true;
                value->dirty = false;
            }
        if (dirty) {
            page->message = tr("ui.unsaved");
            return;
        }
        closing = true;
    }
    void observeLayout() {
        auto current = scene.lock();
        if (!current || !current->mScreenView) return;
        auto const& view = *current->mScreenView;
        if (!view.mVisualTree || !view.mVisualTree->mRootControl) return;
        auto const& root  = *view.mVisualTree->mRootControl;
        auto        size  = root.mSize.get();
        std::string state = "root=" + std::to_string(size.x) + "x" + std::to_string(size.y)
                          + " children=" + std::to_string(root.mChildren->size())
                          + " renderables=" + std::to_string(view.mRenderableControls->size());
        unsigned    count = 0;
        for (auto const& child : *root.mChildren) {
            if (!child || ++count > 3) continue;
            auto childSize = child->mSize.get();
            state += " [" + *child->mName + " " + std::to_string(childSize.x) + "x" + std::to_string(childSize.y)
                   + " children=" + std::to_string(child->mChildren->size()) + "]";
        }
        std::lock_guard lock(requestMutex);
        layoutState = std::move(state);
    }
    void bindText(std::string name, std::function<std::string()> get) {
        bindString(StringHash{std::string_view{name}}, [get = std::move(get)] { return get(); }, [] { return true; });
    }
    void button(std::string name, std::function<void()> action) {
        registerButtonPressedHandler(
            StringHash{std::string_view{name}},
            [this, action = std::move(action)](UIPropertyBag*) {
                pending.push_back(action);
                return ::ui::ViewRequest::Refresh;
            }
        );
    }

public:
    Controller(PageId initial, uint64_t g, uint64_t r) : ScreenController(false), generation(g), resources(r) {
        tabs.active = initial;
        for (size_t i = 0; i < tabs.pages.size(); ++i) tabs.pages[i] = buildPage(static_cast<PageId>(i));
        page = tabs.pages[static_cast<size_t>(initial)];
        for (auto const& value : tabs.pages) {
            auto error = PageBuilder::validate(*value);
            if (!error.empty()) throw std::runtime_error(error);
        }
        bindText("#ls_title", [this] { return "LeviSchematic / " + page->title; });
        bindText("#ls_message", [this] { return page->message; });
        bindText("#ls_edit", [this] { return edited; });
        bindText("#ls_back", [] { return tr("ui.back"); });
        bindText("#ls_commit", [] { return tr("ui.commit"); });
        bindText("#ls_hint", [] { return tr("ui.browse_hint"); });
        bindBool(StringHash{"#ls_editing"}, [this] { return editing.has_value(); }, [] { return true; });
        bindBool(StringHash{"#ls_not_editing"}, [this] { return !editing; }, [] { return true; });
        bindBool(StringHash{"#ls_tabs_focus"}, [] { return true; }, [] { return true; });
        std::array<std::string, 3> suffixes{"projection", "settings", "keys"};
        for (size_t i = 0; i < suffixes.size(); ++i) {
            auto n  = suffixes[i];
            auto id = static_cast<PageId>(i);
            bindText("#ls_tab_" + n, [n] { return tr("ui.tab." + n); });
            bindBool(
                StringHash{std::string_view{"#ls_visible_" + n}},
                [this, id] { return tabs.active == id; },
                [] { return true; }
            );
            bindGridSize(
                StringHash{std::string_view{"#ls_dims_" + n}},
                [this, i] { return glm::ivec2{1, tabs.pages[i] ? static_cast<int>(tabs.pages[i]->rows.size()) : 0}; },
                [] { return true; }
            );
            std::string collection = "ls_rows_" + n;
            bindStringForCollection(
                StringHash{std::string_view{collection}},
                StringHash{"#ls_row_text"},
                [this, i](int index) {
                    if (!tabs.pages[i]) return std::string{};
                    auto const& rows = tabs.pages[i]->rows;
                    if (index < 0 || static_cast<size_t>(index) >= rows.size()) return std::string{};
                    auto const& row = rows[index];
                    return row.label + (row.editable ? ": " + row.value : "");
                },
                [](int) { return true; }
            );
            bindStringForCollection(
                StringHash{std::string_view{collection}},
                StringHash{"#ls_row_id"},
                [this, i](int index) {
                    if (!tabs.pages[i]) return std::string{};
                    auto const& rows = tabs.pages[i]->rows;
                    return index >= 0 && static_cast<size_t>(index) < rows.size() ? rows[index].id : std::string{};
                },
                [](int) { return true; }
            );
            bindBoolForCollection(
                StringHash{std::string_view{collection}},
                StringHash{"#ls_row_selected"},
                [this, id, i](int index) {
                    if (tabs.active != id || !editing || !tabs.pages[i]) return false;
                    auto const& rows = tabs.pages[i]->rows;
                    return index >= 0 && static_cast<size_t>(index) < rows.size() && rows[index].id == *editing;
                },
                [](int) { return true; }
            );
            registerButtonPressedHandler(
                StringHash{std::string_view{"button.ls_row_" + n}},
                [this, id, i](UIPropertyBag* bag) {
                    if (!bag || tabs.active != id) return ::ui::ViewRequest::None;
                    auto rowId  = bag->mJsonValue.get()["#ls_row_id"].asString("");
                    auto source = tabs.pages[i];
                    if (!source) return ::ui::ViewRequest::None;
                    auto row = findRow(*source, rowId);
                    if (!row) return ::ui::ViewRequest::None;
                    Row selected = *row;
                    pending.push_back([this, source, selected = std::move(selected)] {
                        if (page != source || !findRow(*source, selected.id)) return;
                        if (selected.editable) {
                            editing = selected.id;
                            edited  = selected.value;
                        } else selected.submit({});
                    });
                    return ::ui::ViewRequest::Refresh;
                }
            );
        }
        registerToggleChangeEventHandler(StringHash{"ls_tabs"}, [this](ToggleChangeEventData& event) {
            
            int index = event.index;
            if (!event.state || !event.toggledByButtonClick || index < 0
                || static_cast<size_t>(index) >= tabs.pages.size())
                return ::ui::ViewRequest::None;
            pending.push_back([this, index] {
                auto id = static_cast<PageId>(index);
                if (closing || tabs.active == id || !tabs.pages[index]) return;
                page->capturing = false;
                editing.reset();
                edited.clear();
                tabs.active = id;
                page        = tabs.pages[index];
            });
            return ::ui::ViewRequest::Refresh;
        });
        button("button.ls_back", [this] { back(); });
        button("button.ls_commit", [this] {
            if (!editing) return;
            for (auto const& row : page->rows)
                if (row.id == *editing) {
                    auto submit = row.submit;
                    submit(edited);
                    break;
                }
            editing.reset();
        });
        registerTextEditChangedEventHandler(
            StringHash{std::string_view{"ls_editor"}},
            [this](TextEditScreenEventData& event, int) {
                if (editing && event.properties) {
                    auto const& data  = event.properties->mJsonValue.get();
                    auto        field = data["#property_field"].asString("#text_box_text");
                    edited            = data[field].asString(edited);
                }
                return ::ui::ViewRequest::Refresh;
            }
        );
    }
    ~Controller() override {
        tabs.clear();
        opened = false;
    }
    void              attachScene(std::shared_ptr<UIScene> const& current) { scene = current; }
    bool              _doesScreenHaveExitBehavior() const override { return true; }
    ::ui::ViewRequest tryExit() override {
        pending.push_back([this] { back(); });
        return ::ui::ViewRequest::ConsumeEvent;
    }
    void onInit() override {
        ScreenController::onInit();
        report("controller initialized");
    }
    void onOpen() override {
        ScreenController::onOpen();
        if (auto client = ll::service::getClientInstance()) client->releaseMouse();
        report("controller opened");
    }
    void onTerminate() override {
        closing = true;
        pending.clear();
        page->capturing = false;
        tabs.clear();
        opened = false;
        {
            std::lock_guard lock(requestMutex);
            requests.didClose();
        }
        report("controller terminated");
        ScreenController::onTerminate();
    }
    ::ui::ViewRequest handleEvent(ScreenEvent& event) override {
        if (event.type == ScreenEventType::RawInputEvent) {
            auto const& raw = event.data->rawInput;
            if (raw->state == ButtonState::Down && raw->keyType == RawInputType::Keyboard && raw->id == 27) {
                pending.push_back([this] { back(); });
                return ::ui::ViewRequest::ConsumeEvent;
            }
        }
        if (page->capturing && event.type == ScreenEventType::RawInputEvent) {
            auto const& raw = event.data->rawInput;
            if (raw->state == ButtonState::Down) {
                int key  = raw->id;
                int type = static_cast<int>(raw->keyType);
                pending.push_back([p = page, key, type] {
                    if (type == 4 && key == 27) p->capturing = false;
                    else if (p->rawKey) p->rawKey(key, type);
                });
            }
            return ::ui::ViewRequest::ConsumeEvent;
        }
        return ScreenController::handleEvent(event);
    }
    ::ui::DirtyFlag tick() override {
        backHandled = false;
        if (++observationTicks == 2 || observationTicks == 30 || observationTicks % 120 == 0) observeLayout();
        if (++tickCount == 1) report("controller tick reached");
        bool valid = enabled;
        {
            
            
            auto session = app::accessSession();
            valid        = valid && session && session->sessionGeneration() == generation
                        && session->resourceGeneration() == resources;
        }
        if (!valid) {
            pending.clear();
            if (!closing) report("closing: session or resource generation changed");
            closing = true;
        }
        {
            std::lock_guard lock(requestMutex);
            if (requests.page || requests.close) closing = true;
        }
        if (!closing) {
            auto work = std::move(pending);
            pending.clear();
            for (auto& action : work) {
                try {
                    action();
                } catch (std::exception const& e) {
                    page->message = e.what();
                }
            }
            if (page->refresh) page->refresh();
        }
        if (closing && !closeScheduled) {
            if (auto client = ll::service::getClientInstance())
                client->getClientSceneStack()->schedulePopScreenWithExpectedNames({"levischematic.screen"});
            closeScheduled = true;
        }
        ScreenController::tick();
        return ::ui::DirtyFlag::UpdateBinds;
    }
};
} 
void start() {
    if (!enabled.exchange(true)) {
        (void)definition();
        DefinitionHook::hook();
        report("definition provider installed");
    }
}
void stop() {
    enabled = false;
    {
        std::lock_guard lock(requestMutex);
        requests.clear();
    }
    if (auto client = ll::service::getClientInstance(); client && opened)
        client->getClientSceneStack()->schedulePopScreenWithExpectedNames({"levischematic.screen"});
    DefinitionHook::unhook();
}
bool isOpen() { return opened; }
void requestOpen(PageId id) {
    std::lock_guard lock(requestMutex);
    ++requestCount;
    if (enabled) {
        requests.open(id);
        lastEvent = "open requested";
    } else lastEvent = "open rejected: UI disabled";
}
void requestToggle() {
    std::lock_guard lock(requestMutex);
    ++requestCount;
    if (!enabled) {
        lastEvent = "toggle rejected: UI disabled";
        return;
    }
    requests.toggle(opened);
    lastEvent = requests.page ? "open requested by toggle" : "close requested by toggle";
}
std::string diagnostics() {
    std::lock_guard lock(requestMutex);
    return "enabled=" + std::to_string(enabled.load()) + " pending=" + std::to_string(requests.page.has_value())
         + " closePending=" + std::to_string(requests.close) + " opened=" + std::to_string(opened.load())
         + " requests=" + std::to_string(requestCount.load()) + " consumed=" + std::to_string(consumedCount.load())
         + " definitionQueries=" + std::to_string(definitionCount.load()) + " ticks=" + std::to_string(tickCount.load())
         + " last=" + lastEvent + " layout=" + layoutState;
}
void pump() {
    if (enabled)
        if (auto client = ll::service::getClientInstance()) synchronizeBindings(*client);
    if (opened) return;
    std::optional<PageId> id;
    {
        std::lock_guard lock(requestMutex);
        id = std::exchange(requests.page, {});
    }
    if (!id || !enabled || opened) return;
    ++consumedCount;
    report("processing open request");
    auto client = ll::service::getClientInstance();
    if (!client) {
        report("open failed: no client");
        return;
    }
    uint64_t generation, resources;
    {
        auto session = app::accessSession();
        if (!session) {
            report("open failed: no active session");
            return;
        }
        generation = session->sessionGeneration();
        resources  = session->resourceGeneration();
    }
    try {
        reloadTranslations();
        auto  controller = std::make_shared<Controller>(*id, generation, resources);
        auto& factory    = client->getSceneFactory();
        auto  uiScene    = factory.createUIScene("levischematic.screen", controller);
        if (!uiScene) {
            report("open failed: createUIScene returned null");
            return;
        }
        controller->attachScene(uiScene);
        
        
        auto scene = factory._createScreen(std::move(uiScene));
        if (!scene) {
            report("open failed: _createScreen returned null");
            return;
        }
        report("scene prepared; definition queries=" + std::to_string(definitionCount.load()));
        opened = true;
        
        
        auto stack = client->getClientSceneStack();
        stack->pushScreen(std::move(scene), false);
        report("scene queued on client stack; top=" + stack->getScreenName());
    } catch (std::exception const& e) {
        opened = false;
        report(std::string{"open failed: "} + e.what());
    }
}
} 
