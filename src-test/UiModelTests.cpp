#include "levischematic/ui/OpenRequests.h"
#include "levischematic/ui/Ui.h"
#include "levischematic/ui/UiCatalog.h"
#include "levischematic/ui/UiLayout.h"
#include <cassert>
#include <cmath>
#include <unordered_set>

using namespace levischematic::ui;
namespace {
using J = nlohmann::ordered_json;
J const& child(J const& parent, std::string const& name) {
    for (auto const& entry : parent.at("controls"))
        if (entry.contains(name)) return entry.at(name);
    assert(false && "Missing control");
    return parent;
}
float dimension(J const& value, float parent) {
    if (value.is_number()) return value.get<float>();
    auto expression = value.get<std::string>();
    std::erase(expression, ' ');
    auto percent = expression.find('%');
    assert(percent != std::string::npos);
    float offset = percent + 1 == expression.size() ? 0 : std::stof(expression.substr(percent + 1));
    return parent * std::stof(expression) / 100 + offset;
}
struct Rect {
    float x, y, width, height;
};
Rect layout(J const& control, float width, float height) {
    auto size = control.value("size", J::array({"100%", "100%"}));
    Rect r{0, 0, dimension(size[0], width), dimension(size[1], height)};
    if (control.contains("max_size")) {
        r.width  = std::min(r.width, dimension(control.at("max_size")[0], width));
        r.height = std::min(r.height, dimension(control.at("max_size")[1], height));
    }
    auto anchor = control.value("anchor_from", "center");


    assert(anchor == control.value("anchor_to", "center"));
    r.x = anchor.ends_with("left") ? 0 : anchor.ends_with("right") ? width - r.width : (width - r.width) / 2;
    r.y = anchor.starts_with("top") ? 0 : anchor.starts_with("bottom") ? height - r.height : (height - r.height) / 2;
    auto offset  = control.value("offset", J::array({0, 0}));
    r.x         += offset[0].get<float>();
    r.y         += offset[1].get<float>();
    return r;
}
void contained(Rect r, float width, float height) {
    assert(r.x >= -0.01f && r.y >= -0.01f && r.width > 0 && r.height > 0);
    assert(r.x + r.width <= width + 0.01f && r.y + r.height <= height + 0.01f);
}
void checkBindings(nlohmann::ordered_json const& control) {
    if (control.value("type", "") == "label") {
        auto text = control.value("text", "");
        if (text.starts_with('#')) {
            bool supplied = false;
            for (auto const& binding : control.at("bindings")) {
                auto target = binding.value("binding_name_override", binding.value("binding_name", ""));
                if (target == text) supplied = true;
            }

            assert(supplied);
        }
    }
    if (control.contains("controls"))
        for (auto const& child : control.at("controls"))
            for (auto const& [name, definition] : child.items()) checkBindings(definition);
}
} 
int main() {
    OpenRequests requests;
    requests.toggle(false);
    assert(requests.page == PageId::Projection && !requests.close);
    requests.page.reset();
    requests.toggle(true);
    assert(!requests.page && requests.close);
    requests.didClose();
    requests.toggle(false);
    assert(requests.page == PageId::Projection && !requests.close);
    requests.toggle(false);
    assert(!requests.page && !requests.close);
    requests.toggle(true);
    requests.toggle(true);
    requests.didClose();
    assert(requests.page == PageId::Projection && !requests.close);
    requests.clear();
    requests.open(PageId::Settings);
    assert(requests.page == PageId::Settings && !requests.close);

    auto        definitions = buildNativeDefinitions();
    auto const& definition  = definitions.at("screen");
    assert(!definition.at("should_steal_mouse").get<bool>());
    checkBindings(definition);
    assert(definition.at("controls").at(0).contains("screen_background@common.screen_background"));
    auto const& panel = child(definition, "panel");
    auto const& strip = child(panel, "tab_strip");
    auto const& body  = child(panel, "body");
    assert(strip.at("type") == "stack_panel" && strip.at("orientation") == "horizontal");
    assert(strip.at("controls").size() == 3);
    assert(strip.at("layer") > body.at("layer"));
    child(body, "background@common.dialog_background_opaque");
    auto const& editor = child(body, "editor@common.text_edit_box");
    auto const& commit = child(body, "commit@common_buttons.light_text_button");
    assert(editor.at("bindings").at(0).at("binding_name") == "#ls_editing");
    assert(commit.at("bindings").at(0).at("binding_name_override") == "#visible");
    assert(!editor.at("visible").get<bool>() && !commit.at("visible").get<bool>());


    for (auto viewport : {
             std::array{481.f, 273.f},
             std::array{320.f, 240.f},
             std::array{960.f, 540.f}
    }) {
        auto window = layout(panel, viewport[0], viewport[1]);
        contained(window, viewport[0], viewport[1]);
        auto tabArea     = layout(strip, window.width, window.height);
        auto contentArea = layout(body, window.width, window.height);
        contained(tabArea, window.width, window.height);
        contained(contentArea, window.width, window.height);
        assert(tabArea.y + tabArea.height > contentArea.y);
        for (auto const& entry : body.at("controls"))
            contained(
                layout(entry.begin().value(), contentArea.width, contentArea.height),
                contentArea.width,
                contentArea.height
            );
        auto well = layout(
            child(body, "scroll_projection@common.container_scroll_panel"),
            contentArea.width,
            contentArea.height
        );
        auto field   = layout(editor, contentArea.width, contentArea.height);
        auto submit  = layout(commit, contentArea.width, contentArea.height);
        auto message = layout(child(body, "message"), contentArea.width, contentArea.height);
        assert(message.y + message.height <= well.y);
        assert(well.y + well.height + 4 <= field.y);
        assert(field.x + field.width < submit.x);
        float tabWidth = 0;
        for (auto const& entry : strip.at("controls"))
            tabWidth += layout(entry.begin().value(), tabArea.width, tabArea.height).width;
        assert(std::abs(tabWidth - tabArea.width) < 0.01f);
    }
    int index = 0;
    for (auto const* suffix : {"projection", "settings", "keys"}) {
        std::string n   = suffix;
        auto const& tab = child(strip, "tab_" + n + "@common_tabs.tab_top");
        assert(!tab.contains("controls"));
        assert(tab.at("$radio_toggle_group").get<bool>());
        assert(tab.at("$toggle_name") == "ls_tabs");
        assert(tab.at("$toggle_group_forced_index") == index++);
        assert(tab.at("$toggle_state_binding_name") == "#ls_visible_" + n);
        assert(tab.at("$tab_content") == "levischematic.tab_caption");
        assert(tab.at("$ls_tab_caption") == "#ls_tab_" + n);
        auto const& scroll = child(body, "scroll_" + n + "@common.container_scroll_panel");
        assert(scroll.at("visible").is_boolean());
        assert(scroll.at("bindings").at(0).at("binding_name") == "#ls_visible_" + n);
        assert(scroll.at("bindings").at(0).at("binding_name_override") == "#visible");
        assert(scroll.at("$show_background").get<bool>());
        assert(scroll.at("$scroll_view_port_clips_children").get<bool>());
        assert(scroll.at("$scrolling_panel_base_button_mappings").empty());
        assert(!scroll.at("$scroll_panel_consume_hover_events").get<bool>());
        assert(scroll.at("$scrolling_content") == "levischematic.content_" + n);
        auto const& grid = definitions.at("content_" + n);
        assert(grid.at("type") == "grid" && grid.at("size").at(1) == "default");
        assert(grid.at("grid_item_template") == "levischematic.row_" + n);
        assert(grid.at("grid_dimension_binding") == "#ls_dims_" + n);
        assert(grid.at("collection_name") == "ls_rows_" + n);
        auto const& row    = definitions.at("row_" + n);
        auto const& action = child(row, "action@common_buttons.light_text_button");
        assert(action.at("$button_text_binding_type") == "collection");
        assert(action.at("$button_text_grid_collection_name") == grid.at("collection_name"));
        assert(!action.contains("controls"));
        assert(action.at("bindings").at(0).at("binding_name") == "#ls_row_id");
        assert(action.at("bindings").at(0).at("binding_collection_name") == grid.at("collection_name"));
        auto const& selection = child(row, "selection@common.focus_border_white");
        assert(selection.at("bindings").at(0).at("binding_name") == "#ls_row_selected");
        assert(selection.at("bindings").at(0).at("binding_name_override") == "#visible");
    }
    std::unordered_set<std::string_view> translations;
    for (auto const& entry : uiTranslations) {
        assert(!entry.en.empty() && !entry.zh.empty());
        assert(translations.insert(entry.key).second);
    }
    auto page = std::make_shared<Page>();
    assert(!PageBuilder::validate(*page).empty());
    page->title        = "Test";
    int         clicks = 0;
    std::string committed;
    PageBuilder builder{page};
    builder.button("load", "Load", [&] { ++clicks; });
    builder.input("x", "X", "-16", [&](std::string s) { committed = std::move(s); });
    assert(PageBuilder::validate(*page).empty());
    page->rows[0].submit({});
    page->rows[1].submit("-17");
    assert(clicks == 1 && committed == "-17");
    std::swap(page->rows[0], page->rows[1]);
    assert(findRow(*page, "load")->label == "Load");
    assert(!findRow(*page, "removed"));
    std::swap(page->rows[0], page->rows[1]);
    builder.button("x", "Duplicate", [] {});
    assert(!PageBuilder::validate(*page).empty());
    page->rows.pop_back();
    page->rows[0].id.clear();
    assert(!PageBuilder::validate(*page).empty());
    page->rows[0].id     = "load";
    page->rows[0].submit = {};
    assert(!PageBuilder::validate(*page).empty());

    std::weak_ptr<Page> released;
    {
        PageTabs tabs;
        auto     draft = std::make_shared<Page>();
        draft->dirty   = true;
        draft->rows.push_back({"draft", "Draft", "42", true, [draft](std::string) { draft->dirty = true; }});
        released      = draft;
        tabs.pages[1] = std::move(draft);
        tabs.active   = PageId::Settings;
        tabs.active   = PageId::Projection;
        tabs.active   = PageId::Settings;
        assert(tabs.pages[1]->dirty && tabs.pages[1]->rows[0].value == "42");
    }
    assert(released.expired());
}
