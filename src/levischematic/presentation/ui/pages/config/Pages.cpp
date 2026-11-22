#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/app/lifecycle/AppKernel.h"
#include "levischematic/presentation/hud/ProjectionHud.h"
#include "levischematic/presentation/ui/bindings/Bindings.h"
#include "levischematic/presentation/ui/i18n/UiI18n.h"
#include "levischematic/presentation/ui/model/Ui.h"
#include "levischematic/presentation/ui/pages/advanced/AdvancedPages.h"
#include "levischematic/presentation/ui/pages/browser/BrowserPages.h"
#include "levischematic/presentation/ui/pages/common/PageCommon.h"
#include "levischematic/presentation/ui/pages/core/CorePages.h"
#include "levischematic/presentation/ui/pages/organization/OrganizationPages.h"
#include "levischematic/presentation/ui/pages/placement/PlacementPages.h"
#include "levischematic/presentation/ui/pages/selection/SelectionPages.h"
#include "levischematic/presentation/ui/pages/survival/SurvivalPages.h"
#include "levischematic/shared/i18n/I18n.h"
#include "ll/api/Config.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/registry/ItemRegistryManager.h"
#include <algorithm>
#include <map>
#include <sstream>

namespace levischematic::ui {
namespace {
using J = nlohmann::ordered_json;
J configJson(app::Config const& config) {
    auto result = ll::reflection::serialize<J>(config);
    if (!result) throw std::runtime_error("Cannot serialize configuration");
    return std::move(*result);
}
std::string fieldLabel(std::string const& path) {
    std::map<std::string, std::string> const labels{
        {"/visibility/renderDistanceBlocks",    tr("ui.title.distance")                   },
        {"/visibility/frustumCulling",          tr("ui.title.frustum")                    },
        {"/projection/alpha",                   tr("ui.title.alpha")                      },
        {"/tool/item",                          tr("ui.config.tool_item")                 },
        {"/hull/alwaysShowMissing",             tr("ui.config.always_show_missing_blocks")},
        {"/hull/alwaysShowExtra",               tr("ui.config.always_show_extra_blocks")  },
        {"/hull/expansion",                     tr("ui.title.expansion")                  },
        {"/hull/depthBias",                     tr("ui.title.depth")                      },
        {"/hull/slopeBias",                     tr("ui.title.slope")                      },
        {"/performance/tickBudgetMs",           tr("ui.title.tick")                       },
        {"/performance/frameBudgetMs",          tr("ui.title.frame")                      },
        {"/performance/maxLoadRequests",        tr("ui.title.loads")                      },
        {"/performance/geometryBuildsPerFrame", tr("ui.title.builds")                     },
        {"/performance/cameraResortsPerFrame",  tr("ui.title.resorts")                    },
        {"/performance/queuedChunkBuilds",      tr("ui.title.queue")                      },
        {"/diagnostics",                        tr("ui.title.diagnostics")                },
        {"/hud/enabled",                        tr("ui.title.hud_enabled")                },
        {"/survival/pickBlock",                 tr("ui.config.pick_block")                },
        {"/survival/easyPlace",                 tr("ui.config.easy_place")                },
        {"/survival/placementRestriction",      tr("ui.config.placement_restriction")     },
        {"/survival/holdToPlace",               tr("ui.config.hold_to_place")             },
        {"/survival/materialHud",               tr("ui.config.material_list_hud")         },
        {"/survival/verifierHud",               tr("ui.config.verifier_hud")              },
        {"/survival/swapDelayMs",               tr("ui.config.swap_delay_ms")             },
    };
    if (auto it = labels.find(path); it != labels.end()) return it->second;
    auto label = path.substr(path.find_last_of('/') + 1);
    for (size_t i = 1; i < label.size(); ++i)
        if (label[i] >= 'A' && label[i] <= 'Z') label.insert(i++, 1, ' ');
    return label;
}
std::string fieldTip(std::string const& path) {
    std::map<std::string, std::string> const tips{
        {"/survival/pickBlock",            tr("ui.config.pick_block_tip")                      },
        {"/survival/easyPlace",            tr("ui.config.the_use_key_places_the_projection_s") },
        {"/survival/placementRestriction", tr("ui.config.only_allow_placing_blocks_that_match")},
        {"/survival/holdToPlace",          tr("ui.config.keep_placing_while_the_use_key_is")   },
        {"/survival/materialHud",          tr("ui.config.show_the_most_missing_materials_on")  },
        {"/survival/verifierHud",          tr("ui.config.show_nearby_verifier_errors_on")      },
        {"/survival/swapDelayMs",          tr("ui.config.delay_after_swapping_hotbar_slots")   },
        {"/tool/item",                     tr("ui.config.item_used_as_the_schematic_tool")     },
    };
    auto it = tips.find(path);
    return it == tips.end() ? std::string{} : it->second;
}

void settings(std::shared_ptr<Page> const& p, std::shared_ptr<J> const& draft) {
    p->rows.clear();
    p->title  = tr("ui.config.configuration");
    p->layout = PageLayout::Config;
    PageBuilder b{p};
    b.button("hud_layout", tr("ui.config.edit_hud_layout"), [] {
         requestOpen(PageId::HudLayout);
     }).tip(tr("ui.config.drag_the_info_panels_to_move_and"));
    auto flat = draft->flatten();
    b.input(
        "hud_position",
        tr("ui.title.hud_position"),
        (*draft)["hud"]["position"] == "bottom_right" ? "1" : "0",
        [p, draft](std::string text) {
            if (text != "0" && text != "1") return;
            (*draft)["hud"]["position"] = text == "1" ? "bottom_right" : "bottom_left";
            p->dirty                    = true;
            for (auto& row : p->rows)
                if (row.id == "hud_position") row.value = text;
        }
    );
    p->rows.back().kind    = RowKind::Choice;
    p->rows.back().choices = {tr("ui.hud.left"), tr("ui.hud.right")};
    for (auto const& [path, value] : flat.items()) {
        if (path == "/projection/texture" || path.starts_with("/projection/color/")
            || path.starts_with("/hull/missingColor/") || path == "/version" || path.starts_with("/hud/")
            || path.starts_with("/selection/faceColor/") || path.starts_with("/hull/blockColor/")
            || path.starts_with("/hull/propertyColor/") || path.starts_with("/hull/extraColor/")
            || path.starts_with("/survival/hotbarSlots/"))
            continue;
        b.input(
            path,
            fieldLabel(path),
            value.is_string() ? value.get<std::string>() : value.dump(),
            [p, draft, path, value](std::string text) {
                try {
                    J parsed = value.is_string() ? J(text) : J::parse(text);
                    if (parsed.type() != value.type() && !(parsed.is_number() && value.is_number())) {
                        p->message = tr("ui.type_mismatch") + path;
                        return;
                    }
                    (*draft)[J::json_pointer(path)] = parsed;
                    p->dirty                        = true;
                    for (auto& row : p->rows)
                        if (row.id == path) row.value = parsed.is_string() ? parsed.get<std::string>() : parsed.dump();
                } catch (std::exception const& e) {
                    p->message = e.what();
                }
            }
        );
        auto& row = p->rows.back();
        if (value.is_boolean()) row.kind = RowKind::Toggle;
        else if (value.is_number()) {
            if (path == "/projection/alpha") {
                row.kind         = RowKind::Slider;
                row.displayScale = 100;
                row.unit         = "%";
            } else if (path == "/hull/expansion") {
                row.kind    = RowKind::Slider;
                row.maximum = 0.1;
                row.step    = 0.001;
            } else if (path.starts_with("/performance/")) {
                row.kind    = RowKind::Slider;
                bool budget = path.ends_with("BudgetMs");
                row.minimum = budget ? 0.1 : 1;
                row.maximum = budget ? 50 : path.ends_with("cameraResortsPerFrame") ? 1024 : 64;
                row.step    = budget ? 0.1 : 1;
            }
        }
    }
    std::string slots;
    for (auto const& slot : (*draft)["survival"]["hotbarSlots"]) {
        if (!slots.empty()) slots += ",";
        slots += std::to_string(slot.get<int>() + 1);
    }
    b.input("/survival/hotbarSlotsText", tr("ui.config.pick_hotbar_slots"), slots, [p, draft](std::string text) {
         std::vector<int>   values;
         std::istringstream stream(text);
         std::string        part;
         while (std::getline(stream, part, ',')) {
             int n;
             if (!parseInt(part, n)) {
                 p->message = tr("ui.config.comma_separated_slots_1_9");
                 return;
             }
             values.push_back(n - 1);
         }
         (*draft)["survival"]["hotbarSlots"] = values;
         p->dirty                            = true;
     }).tip(tr("ui.config.hotbar_slots_pick_block_may_replace"));
    for (auto const& [path, title] : {
             std::pair{"/projection/color",    "ui.title.projection_color"},
             {"/selection/faceColor", "ui.title.selection_color" },
             {"/hull/missingColor",   "ui.title.missing_color"   },
             {"/hull/blockColor",     "ui.title.block_color"     },
             {"/hull/propertyColor",  "ui.title.property_color"  },
             {"/hull/extraColor",     "ui.title.extra_color"     }
    }) {
        auto color = (*draft)[J::json_pointer(path)].get<std::array<double, 4>>();
        b.input(path, tr(title), formatHexColor(color), [p, draft, path = std::string(path)](std::string text) {
            auto color = parseHexColor(std::move(text));
            if (!color) {
                p->message = tr("ui.config.color_format_aarrggbb");
                return;
            }
            (*draft)[J::json_pointer(path)] = *color;
            p->dirty                        = true;
            for (auto& row : p->rows)
                if (row.id == path) {
                    row.color = *color;
                    row.value = formatHexColor(*color);
                }
        });
        auto& row       = p->rows.back();
        row.kind        = RowKind::Color;
        row.color       = color;
        row.colorSubmit = [](int, double) {};
        row.tooltip     = tr("ui.config.format_aarrggbb_aa_is_the_opacity");
    }
    for (auto& row : p->rows) {
        if (row.tooltip.empty()) {
            row.tooltip = fieldTip(row.id);
            if (row.tooltip.empty() && row.id.starts_with('/')) row.tooltip = describeRow(row);
        }
        if (row.id.starts_with('/') && row.id != "/survival/hotbarSlotsText") {
            row.reset = [p, draft, path = row.id] {
                auto defaults                   = configJson(app::Config{});
                (*draft)[J::json_pointer(path)] = defaults[J::json_pointer(path)];
                settings(p, draft);
                p->dirty = true;
            };
        }
    }
    mainMenuButton(b);

    auto published = std::make_shared<J>(configJson(app::configStore().snapshot()));
    auto valid     = std::make_shared<bool>(true);
    p->refresh     = [p, draft, published, valid] {
        if (*draft == *published) {
            *valid = true;
            return;
        }
        app::Config config;
        auto        decoded = ll::reflection::deserialize(config, *draft);
        auto        error   = decoded ? app::validateConfig(config) : tr("ui.invalid_config");
        if (error.empty() && app::accessSession()
            && !ItemRegistryManager::getItemRegistry().lookupByNameNoAlias(std::string_view(config.tool.item)))
            error = tr("ui.config.unknown_tool_item", config.tool.item);
        if (error.empty()) error = app::configStore().submit(config);
        *valid = error.empty();
        if (!*valid) {
            p->message = error;
            return;
        }
        *published = *draft;
        p->dirty   = true;
        p->message.clear();
    };
    p->save = [p, valid] {
        if (p->refresh) p->refresh();
        if (!*valid) return false;
        if (!p->dirty) return true;
        auto error = app::configStore().save();
        if (!error.empty()) {
            p->message = tr("ui.config_save_failed") + error;
            return false;
        }
        p->dirty = false;
        return true;
    };
}
std::vector<SpritePart> settingParts(input::KeySettings const& s, bool modified) {
    using input::Activation;
    using input::KeyContext;
    std::vector<SpritePart> parts;
    if (s.context == KeyContext::InGame) parts.push_back({38, 26, 8, 5, 1, 7});
    else if (s.context == KeyContext::Gui) parts.push_back({83, 2, 5, 5, 2, 7});
    else parts.push_back({38, 8, 8, 5, 1, 7});
    if (s.activateOn == Activation::Press) parts.push_back({11, 8, 6, 5, 11, 8});
    else if (s.activateOn == Activation::Release) parts.push_back({11, 26, 6, 5, 11, 8});
    else {
        parts.push_back({11, 26, 6, 5, 11, 6});
        parts.push_back({11, 8, 6, 5, 11, 11});
    }
    if (s.cancel) parts.push_back({83, 20, 5, 5, 12, 0});
    if (modified) {
        parts.push_back({21, 14, 5, 2, 1, 14});
        parts.push_back({28, 14, 5, 2, 7, 14});
    } else parts.push_back({21, 32, 5, 2, 1, 14});
    return parts;
}
std::string yesNo(bool value) { return value ? tr("ui.config.yes") : tr("ui.config.no"); }
std::string activationName(input::Activation a) {
    return a == input::Activation::Press   ? tr("ui.config.press")
         : a == input::Activation::Release ? tr("ui.config.release")
                                           : tr("ui.config.both");
}
std::string contextName(input::KeyContext c) {
    return c == input::KeyContext::InGame ? tr("ui.config.in_game")
         : c == input::KeyContext::Gui    ? tr("ui.config.in_screens")
                                          : tr("ui.config.anywhere");
}
std::string settingsTip(input::KeySettings const& s) {
    return tr("ui.config.advanced_hotkey_settings") + "\n" + tr("ui.config.activate_on") + activationName(s.activateOn)
         + "\n" + tr("ui.config.allow_empty") + yesNo(s.allowEmpty) + "\n" + tr("ui.config.allow_extra_keys")
         + yesNo(s.allowExtraKeys) + "\n" + tr("ui.config.order_sensitive") + yesNo(s.orderSensitive) + "\n"
         + tr("ui.config.cancel_further_processing") + yesNo(s.cancel) + "\n" + tr("ui.config.exclusive")
         + yesNo(s.exclusive) + "\n" + tr("ui.config.context") + contextName(s.context) + "\n\n"
         + tr("ui.config.left_click_configure_right_click");
}
void keys(std::shared_ptr<Page> const& p);
void keySettingsDialog(std::shared_ptr<Page> const& p, input::ActionId id) {
    auto const& spec     = input::actionSpec(id);
    auto        settings = bindingSettings(id);
    beginModal(p, {});
    p->modalTitle = tr("ui.config.advanced_settings", tr(spec.label));
    auto apply    = [p, id](auto change) {
        auto c = ll::service::getClientInstance();
        if (!c) return;
        auto value = bindingSettings(id);
        change(value);
        auto error = saveBindingSettings(*c, id, value);
        dismissModal(p);
        p->rows.clear();
        keys(p);
        keySettingsDialog(p, id);
        if (!error.empty()) p->message = error;
    };
    PageBuilder b{p};
    b.input(
         "activate_on",
         tr("ui.config.activate_on_2"),
         std::to_string(static_cast<int>(settings.activateOn)),
         [apply](std::string text) {
             int n;
             if (parseInt(text, n) && n >= 0 && n <= 2)
                 apply([n](input::KeySettings& s) { s.activateOn = static_cast<input::Activation>(n); });
         }
    )
        .kind(RowKind::Choice)
        .tip(tr("ui.config.press_when_the_last_key_goes_down"));
    b.last().choices = {tr("ui.config.press"), tr("ui.config.release"), tr("ui.config.both")};
    b.input(
         "context",
         tr("ui.config.context_2"),
         std::to_string(static_cast<int>(settings.context)),
         [apply](std::string text) {
             int n;
             if (parseInt(text, n) && n >= 0 && n <= 2)
                 apply([n](input::KeySettings& s) { s.context = static_cast<input::KeyContext>(n); });
         }
    )
        .kind(RowKind::Choice)
        .tip(tr("ui.config.in_game_no_screen_open_in_screens"));
    b.last().choices = {tr("ui.config.in_game"), tr("ui.config.in_screens"), tr("ui.config.anywhere")};
    auto flag        = [&](char const*        key,
                           std::string const& label,
                           bool               value,
                           bool input::KeySettings::* member,
                           std::string const&         tip) {
        b.toggle(key, label, value, [apply, member] {
             apply([member](input::KeySettings& s) { s.*member = !(s.*member); });
         }).tip(tip);
        b.last().yesNo = true;
    };
    flag(
        "allow_empty",
        tr("ui.config.allow_empty_2"),
        settings.allowEmpty,
        &input::KeySettings::allowEmpty,
        tr("ui.config.when_unbound_activates_when_the_last")
    );
    flag(
        "allow_extra",
        tr("ui.config.allow_extra_keys_2"),
        settings.allowExtraKeys,
        &input::KeySettings::allowExtraKeys,
        tr("ui.config.activates_even_when_other_keys_are")
    );
    flag(
        "order_sensitive",
        tr("ui.config.order_sensitive_2"),
        settings.orderSensitive,
        &input::KeySettings::orderSensitive,
        tr("ui.config.keys_must_be_pressed_in_the_bound")
    );
    flag(
        "exclusive",
        tr("ui.config.exclusive_2"),
        settings.exclusive,
        &input::KeySettings::exclusive,
        tr("ui.config.does_not_activate_while_another")
    );
    flag(
        "cancel",
        tr("ui.config.cancel_further_processing_2"),
        settings.cancel,
        &input::KeySettings::cancel,
        tr("ui.config.the_key_is_not_passed_on_to_the_game")
    );
    b.button("cancel_dialog", tr("ui.config.done"), [p] { dismissModal(p); });
    b.last().id = "cancel";
}
void keys(std::shared_ptr<Page> const& p) {
    p->title  = tr("ui.config.configuration");
    p->layout = PageLayout::Config;
    for (auto const& spec : input::actionSpecs()) {
        if (spec.id == input::ActionId::InfoRelease || spec.id == input::ActionId::EasyPlaceRelease) continue;
        auto action  = bindingActionName(spec.id);
        auto id      = spec.id;
        auto current = bindingKeys(spec.id);
        auto refresh = [p](std::string message) {
            p->message = std::move(message);
            p->rows.clear();
            keys(p);
        };
        PageBuilder{p}.input(action, tr(spec.label), describeBinding(current), [p, action, refresh](std::string) {
            p->capturing     = true;
            p->message       = tr("ui.config.hold_keys_release_to_save_esc_clears");
            p->captureEscape = [action, refresh] {
                auto c = ll::service::getClientInstance();
                if (!c) return;
                auto error = saveBinding(*c, action, {});
                refresh(error.empty() ? tr("ui.key_cleared") : error);
            };
            auto captured = std::make_shared<std::vector<int>>();
            p->rawKey     = [p, action, captured, refresh](int key, int type, bool down) {
                int code = type == 1 ? -key : type == 2 ? input::WheelUp : type == 3 ? input::WheelDown : key;
                if (type != 1 && type != 2 && type != 3 && type != 4) return;
                if (code == 160 || code == 161) code = 16;
                if (code == 162 || code == 163) code = 17;
                if (code == 164 || code == 165) code = 18;
                if (down) {
                    if (std::find(captured->begin(), captured->end(), code) == captured->end())
                        captured->push_back(code);
                    if (type != 2 && type != 3) return;
                }
                if (captured->empty()) return;
                auto c = ll::service::getClientInstance();
                if (!c) return;
                auto error = saveBinding(*c, action, *captured);
                if (!error.empty()) {
                    p->message = error;
                    captured->clear();
                    return;
                }
                p->capturing     = false;
                p->captureEscape = {};
                refresh(tr("ui.key_saved"));
            };
        });
        auto& row        = p->rows.back();
        auto  settings   = bindingSettings(spec.id);
        bool  customised = settings != input::defaultKeySettings(spec);
        row.kind         = RowKind::KeyBinding;
        row.textColor    = current != spec.keys ? colors::Gold : colors::Default;
        row.tooltip      = tr("ui.config.click_then_press_the_new_key");
        row.parts        = settingParts(settings, customised);
        row.partsTooltip = settingsTip(settings);
        row.activate     = [p, id] { keySettingsDialog(p, id); };
        row.secondary    = [id, refresh] {
            auto c = ll::service::getClientInstance();
            if (!c) return;
            auto error = saveBindingSettings(*c, id, input::defaultKeySettings(input::actionSpec(id)));
            refresh(error.empty() ? tr("ui.config.advanced_settings_reset") : error);
        };
        row.clear = [action, refresh] {
            auto c = ll::service::getClientInstance();
            if (!c) return;
            auto error = saveBinding(*c, action, {});
            refresh(error.empty() ? tr("ui.key_cleared") : error);
        };
        row.reset = [action, defaults = spec.keys, refresh] {
            auto c = ll::service::getClientInstance();
            if (!c) return;
            auto error = saveBinding(*c, action, defaults);
            refresh(error.empty() ? tr("ui.key_restored") : error);
        };
    }
    PageBuilder b{p};
    mainMenuButton(b);
}

void mainMenu(std::shared_ptr<Page> const& p) {
    p->title      = tr("ui.config.main_menu");
    p->layout     = PageLayout::Menu;
    p->searchable = false;
    PageBuilder b{p};
    auto        entry =
        [&](PageId id, std::string const& label, int column, int slot, IconRef icon, std::string const& tip = {}) {
            b.button("navigate" + std::to_string(static_cast<int>(id)), label, [id] { requestOpen(id); }).icon(icon);
            b.last().x = static_cast<float>(column);
            b.last().y = static_cast<float>(slot);
            if (!tip.empty()) b.last().tooltip = tip;
        };
    entry(PageId::Projection, tr("ui.config.schematic_placements"), 0, 0, icons::MenuPlacements);
    entry(PageId::Resources, tr("ui.config.loaded_schematics"), 0, 1, icons::MenuLoaded);
    entry(PageId::Load, tr("ui.config.load_schematics"), 0, 2, icons::MenuLoad);
    entry(PageId::Selection, tr("ui.config.area_editor"), 0, 4, icons::MenuAreaEditor);
    entry(PageId::AreaSelections, tr("ui.config.area_selection_browser"), 0, 5, icons::MenuAreaSelections);
    entry(PageId::Settings, tr("ui.config.configuration_menu"), 1, 0, icons::MenuConfig);
    entry(PageId::SchematicManager, tr("ui.config.schematic_manager"), 1, 4, icons::MenuManager);
    entry(PageId::Tasks, tr("ui.config.task_manager"), 1, 5, icons::MenuTasks);
    entry(PageId::Projects, tr("ui.config.building_projects"), 1, 6, {}, tr("ui.config.multi_schematic_projects_and"));
    entry(PageId::Edit, tr("ui.config.edit_schematic"), 0, 7, {}, tr("ui.config.edit_the_projection_s_blocks_with"));
    entry(
        PageId::WorldOperations,
        tr("ui.config.world_operations"),
        1,
        7,
        {},
        tr("ui.config.paste_fill_replace_and_other_world")
    );
    entry(PageId::Signs, tr("ui.config.sign_text"), 0, 8, {}, tr("ui.config.fill_in_the_projection_s_sign_text"));
    entry(PageId::Session, tr("ui.config.session_recovery"), 1, 8, {}, tr("ui.config.restore_or_keep_the_previous"));
    auto s      = app::accessSession();
    bool normal = s && s->selection().state().normal;
    b.input(
         "selection_mode",
         tr("ui.config.selection_mode"),
         normal ? "1" : "0",
         [](std::string) {
             if (auto s = app::accessSession()) s->selection().setNormal(!s->selection().state().normal);
             requestOpen(PageId::MainMenu);
         }
    )
        .kind(RowKind::Choice)
        .tip(tr("ui.config.simple_one_box_normal_several_named"));
    b.last().choices = {tr("ui.config.simple"), tr("ui.config.normal")};
    b.last().x       = 0;
    b.last().y       = 6;
    auto mode        = s ? s->tools().state().activeTool : editor::ToolMode::None;
    bool enabled     = s && s->tools().state().enabled;
    b.button(
         "tool_mode",
         tr("ui.config.tool_mode", tr(app::toolNameKey(mode))),
         [] {
             if (auto s = app::accessSession()) s->tools().cycle(1);
             requestOpen(PageId::MainMenu);
         }
    )
        .in(Slot::FooterLeft)
        .right([] {
            if (auto s = app::accessSession()) s->tools().cycle(-1);
            requestOpen(PageId::MainMenu);
        })
        .tip(tr("ui.config.left_click_next_mode_right_click"));
    b.toggle(
         "tool_toggle",
         tr("ui.config.tool"),
         enabled,
         [] {
             if (auto s = app::accessSession()) s->tools().toggle();
             requestOpen(PageId::MainMenu);
         }
    )
        .in(Slot::FooterLeft)
        .tip(tr("ui.config.when_off_the_tool_item_does_nothing"));
}

void tasks(std::shared_ptr<Page> const& p) {
    p->title  = tr("ui.config.task_manager");
    p->layout = PageLayout::List;
    PageBuilder b{p};
    b.button("refresh", tr("ui.config.refresh"), [] { requestOpen(PageId::Tasks); }).in(Slot::TopBar);
    mainMenuButton(b);
    auto session = app::accessSession();
    if (!session) return;
    auto entry = [&](std::string key, std::string text, std::vector<Row> actions = {}) {
        b.button(key, text, [] {});
        b.last().kind    = RowKind::TableRow;
        b.last().actions = std::move(actions);
    };
    auto action = [](std::string id, std::string label, std::function<void()> fn) {
        Row r;
        r.id     = std::move(id);
        r.label  = std::move(label);
        r.submit = [fn = std::move(fn)](std::string) { fn(); };
        return r;
    };
    auto save = session->saves().progress();
    entry(
        "save_progress",
        tr("ui.config.schematic_save") + ": " + save.phase + " " + std::to_string(save.completed) + "/"
            + std::to_string(save.total) + " " + save.detail,
        {action("save_details", tr("ui.config.details"), [] { requestOpen(PageId::SaveSelection); })}
    );
    auto world = session->worldEdit().progress();
    entry(
        "world_progress",
        tr("ui.config.world_operation") + ": " + tr(worldedit::phaseKey(world.phase)) + " " + world.detail,
        {action("world_details", tr("ui.config.details"), [] { requestOpen(PageId::WorldOperations); })}
    );
    entry(
        "project_progress",
        tr("ui.config.projects") + ": " + session->projects().view().detail,
        {action("project_details", tr("ui.config.details"), [] { requestOpen(PageId::Projects); })}
    );
    if (auto conversion = session->conversions().status()) {
        std::vector<Row> actions;
        if (!conversion->terminal)
            actions.push_back(action("conversion_cancel", tr("ui.config.cancel"), [] {
                if (auto s = app::accessSession()) (void)s->conversions().cancel();
                requestOpen(PageId::Tasks);
            }));
        entry(
            "conversion",
            tr("ui.config.conversion") + ": " + conversion->phase + " " + conversion->detail,
            std::move(actions)
        );
    }
    auto jobs = session->loads().statuses(session->sessionGeneration());
    if (jobs.empty()) b.info("empty", tr("ui.config.no_loading_tasks")).color(colors::Gray);
    for (auto const& job : jobs) {
        auto             key = "task:" + std::to_string(job.id);
        std::vector<Row> actions;
        if (!job.terminal)
            actions.push_back(action(key + ":cancel", tr("ui.config.cancel"), [task = job.id] {
                if (auto s = app::accessSession()) (void)s->loads().cancel(task, s->sessionGeneration());
                requestOpen(PageId::Tasks);
            }));
        entry(
            key,
            tr("ui.config.load") + " #" + std::to_string(job.id) + " " + tr("ui.phase." + job.phase) + " " + job.detail,
            std::move(actions)
        );
    }
}
} // namespace

std::shared_ptr<Page> buildPage(PageId id) {
    auto page   = std::make_shared<Page>();
    page->route = id;
    switch (id) {
    case PageId::HudLayout:
        page->title = tr("ui.config.hud_layout");
        break;
    case PageId::Load:
    case PageId::SchematicManager:
    case PageId::AreaSelections:
        populateBrowserPage(page, id);
        break;
    case PageId::Tasks:
        tasks(page);
        break;
    case PageId::WorldOperations:
    case PageId::Projects:
    case PageId::Signs:
        populateAdvancedPage(page, id);
        break;
    case PageId::Resources:
    case PageId::Edit:
        populateOrganizationPage(page, id);
        break;
    case PageId::Materials:
    case PageId::Survival:
        populateSurvivalPage(page, id);
        break;
    case PageId::SaveSelection:
    case PageId::Layers:
    case PageId::Verifier:
    case PageId::Session:
        populateCorePage(page, id);
        break;
    case PageId::Projection:
    case PageId::Placement:
    case PageId::SubRegion:
    case PageId::PlacementGrid:
        populatePlacementPage(page, id);
        break;
    case PageId::Settings: {
        auto draft = std::make_shared<J>(configJson(app::configStore().editableSnapshot()));
        settings(page, draft);
        break;
    }
    case PageId::MainMenu:
        mainMenu(page);
        break;
    case PageId::Selection:
        populateSelectionPage(page);
        break;
    case PageId::KeyBindings:
        keys(page);
        break;
    }
    return page;
}
} // namespace levischematic::ui
