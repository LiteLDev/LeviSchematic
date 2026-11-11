#include "AdvancedPages.h"
#include "Bindings.h"
#include "CorePages.h"
#include "OrganizationPages.h"
#include "SurvivalPages.h"
#include "Ui.h"
#include "UiI18n.h"
#include "levischematic/LeviSchematic.h"
#include "levischematic/app/AppKernel.h"
#include "levischematic/app/ProjectionRefresh.h"
#include "ll/api/Config.h"
#include "ll/api/input/KeyRegistry.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/input/ClientInputHandler.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/input/Keymapping.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/dimension/Dimension.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <map>

namespace levischematic::ui {
namespace {
using J  = nlohmann::ordered_json;
using PI = placement::PlacementInstance;
J configJson(app::Config const& config) {
    auto result = ll::reflection::serialize<J>(config);
    if (!result) throw std::runtime_error("Cannot serialize configuration");
    return std::move(*result);
}
std::string fieldLabel(std::string const& path) {
    std::map<std::string, std::string> const labels{
        {"/visibility/renderDistanceBlocks",    tr("ui.title.distance")   },
        {"/visibility/frustumCulling",          tr("ui.title.frustum")    },
        {"/projection/alpha",                   tr("ui.title.alpha")      },
        {"/projection/texture",                 tr("ui.title.texture")    },
        {"/hull/expansion",                     tr("ui.title.expansion")  },
        {"/hull/depthBias",                     tr("ui.title.depth")      },
        {"/hull/slopeBias",                     tr("ui.title.slope")      },
        {"/performance/tickBudgetMs",           tr("ui.title.tick")       },
        {"/performance/frameBudgetMs",          tr("ui.title.frame")      },
        {"/performance/maxLoadRequests",        tr("ui.title.loads")      },
        {"/performance/geometryBuildsPerFrame", tr("ui.title.builds")     },
        {"/performance/cameraResortsPerFrame",  tr("ui.title.resorts")    },
        {"/performance/queuedChunkBuilds",      tr("ui.title.queue")      },
        {"/diagnostics",                        tr("ui.title.diagnostics")},
        {"/hud/enabled",                        tr("ui.title.hud_enabled")},
        {"/survival/pickBlock", "取物辅助 / Pick block"},
        {"/survival/easyPlace", "辅助放置 / Easy Place"},
        {"/survival/placementRestriction", "放置限制 / Placement restriction"},
        {"/survival/holdToPlace", "按住持续放置 / Hold to place"},
        {"/survival/materialHud", "材料 HUD / Material HUD"},
        {"/survival/verifierHud", "校验 HUD / Verifier HUD"},
        {"/survival/swapDelayMs", "换物间隔（毫秒） / Swap delay (ms)"},
    };
    if (auto it = labels.find(path); it != labels.end()) return it->second;
    for (auto const& [prefix, label] : {
             std::pair{"/selection/faceColor/", tr("ui.title.selection_color")},
             {"/hull/blockColor/",     tr("ui.title.block_color")    },
             {"/hull/propertyColor/",  tr("ui.title.property_color") }
    }) {
        if (path.starts_with(prefix) && path.back() >= '0' && path.back() <= '3')
            return std::string(label) + " / " + "RGBA"[path.back() - '0'] + tr("ui.unit_color");
    }
    if (path.starts_with("/survival/hotbarSlots/"))
        return localizedText("快捷栏槽位 / Hotbar slot") + " " + path.substr(path.find_last_of('/') + 1);
    auto label = path.substr(path.find_last_of('/') + 1);
    for (size_t i = 1; i < label.size(); ++i)
        if (label[i] >= 'A' && label[i] <= 'Z') label.insert(i++, 1, ' ');
    return label;
}
bool integer(std::string const& text, int& value) {
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}
void confirm(std::shared_ptr<Page> const& p, std::string text, std::function<void()> action) {
    beginModal(p, std::move(text));
    PageBuilder{p}
        .button(
            "yes",
            tr("ui.confirm"),
            [p, action = std::move(action)] {
                dismissModal(p);
                action();
                p->refresh = {};
            }
        )
        .button("cancel", tr("ui.cancel"), [p] {
            dismissModal(p);
        });
}
void projection(std::shared_ptr<Page> const& p) {
    auto s = app::accessSession();
    if (!s) {
        p->message = tr("ui.no_session");
        return;
    }
    p->title = p->placementSettings ? "当前放置 / Selected placement" : tr("ui.tab.projection");
    PageBuilder b{p};
    
    
    if (!p->placementSettings) {
        b.button("refresh", tr("ui.refresh"), [p] {
            p->rows.clear();
            projection(p);
        });
        b.input("file", tr("ui.file_name"), p->fileName, [p](std::string text) {
            p->fileName = std::move(text);
            for (auto& row : p->rows)
                if (row.id == "file") row.value = p->fileName;
        });
        p->rows.back().kind     = RowKind::File;
        p->rows.back().activate = [p] {
            if (p->fileName.empty()) {
                p->message = tr("ui.file_required");
                return;
            }
            auto session = app::accessSession();
            auto client  = ll::service::getClientInstance();
            if (!session || !client || !client->getLocalPlayer()) return;
            auto* player = client->getLocalPlayer();
            auto  result = session->loads().request(
                session->runtime().resolveSchematicPath(p->fileName),
                BlockPos(player->getPosition()),
                static_cast<int>(player->getDimensionId()),
                session->sessionGeneration(),
                app::configStore().snapshot().performance.maxLoadRequests
            );
            if (!result) {
                p->message = result.error();
                return;
            }
            auto task  = result.value();
            p->message = tr("ui.load_queued") + std::to_string(task);
            PageBuilder{p}
                .button(
                    "task:" + std::to_string(task),
                    tr("ui.task_status") + std::to_string(task),
                    [p, task] {
                        auto active = app::accessSession();
                        if (!active) return;
                        auto status = active->loads().status(task, active->sessionGeneration());
                        p->message =
                            status ? tr("ui.phase." + status->phase) + ": " + status->detail : tr("ui.task_missing");
                    }
                )
                .button("cancel:" + std::to_string(task), tr("ui.task_cancel") + std::to_string(task), [p, task] {
                    auto active = app::accessSession();
                    if (!active) return;
                    p->message = active->loads().cancel(task, active->sessionGeneration()) ? tr("ui.cancelled")
                                                                                           : tr("ui.task_finished");
                });
        };
        auto        files    = s->runtime().listAvailableSchematics();
        std::string fileList = tr("ui.available_files");
        if (files.empty()) fileList += "\n" + tr("ui.no_files");
        for (auto const& file : files) fileList += "\n" + file;
        if (p->route == PageId::Load) {
            if (files.empty()) b.info("files", tr("ui.no_files"));
            for (auto const& file : files) {
                b.button("file:" + file, file, [p, file] {
                    p->fileName = file;
                    for (auto& row : p->rows)
                        if (row.id == "file") row.value = file;
                        else if (row.id.starts_with("file:")) row.selected = row.id == "file:" + file;
                });
                p->rows.back().kind = RowKind::TableRow;
            }
            p->title = "加载结构 / Load schematics";
            return;
        }
        for (auto const& ref : s->placement().managedPlacements()) {
            auto const& item = ref.get();
            auto        id   = item.id;
            b.button("placement:" + std::to_string(id), tr("ui.select") + item.name, [p, id] {
                auto session = app::accessSession();
                if (!session) return;
                auto result = session->placement().selectPlacement(id);
                p->message  = result ? tr("ui.selected") + std::to_string(id) : result.error().describe();
                p->rows.clear();
                projection(p);
            });
        }
    }
    auto id = s->placement().selectedPlacementId();
    if (p->placementSettings && !id) p->message = "没有选中放置 / No placement selected";
    if (id) {
        auto mutate = [p, id = *id](auto operation) {
            auto session = app::accessSession();
            if (!session) return;
            auto result = operation(session->placement(), id);
            if (!result) {
                p->message = result.error().describe();
                return;
            }
            app::refreshCurrentClientProjectionState(*session);
            p->message = tr("ui.updated");
            p->rows.clear();
            projection(p);
        };
        auto const* current = s->placement().findPlacement(*id);
        appendPlacementOrganization(p,*id);
        b.input("enabled", tr("ui.enabled"), current && current->enabled ? "true" : "false", [mutate](std::string) {
            mutate([](auto& svc, auto i) { return svc.togglePlacementEnabled(i); });
        });
        p->rows.back().kind = RowKind::Toggle;
        b.input("render", tr("ui.render"), current && current->renderEnabled ? "true" : "false", [mutate](std::string) {
            mutate([](auto& svc, auto i) { return svc.togglePlacementRender(i); });
        });
        p->rows.back().kind = RowKind::Toggle;
        b.input(
            "rotate",
            tr("ui.rotate"),
            std::to_string(current ? static_cast<int>(current->rotation) : 0),
            [mutate](std::string text) {
                int choice;
                if (!integer(text, choice) || choice < 0 || choice > 3) return;
                mutate([choice](auto& svc, auto i) {
                    auto item = svc.findPlacement(i);
                    auto delta =
                        static_cast<PI::Rotation>((choice - (item ? static_cast<int>(item->rotation) : 0) + 4) % 4);
                    return svc.rotatePlacement(i, delta);
                });
            }
        );
        p->rows.back().kind           = RowKind::Choice;
        p->rows.back().choices        = {"0", "90", "180", "270"};
        auto const* selectedPlacement = s->placement().findPlacement(*id);
        b.input(
            "mirror",
            tr("ui.mirror"),
            std::to_string(selectedPlacement ? static_cast<int>(selectedPlacement->mirror) : 0),
            [mutate](std::string text) {
                int choice;
                if (!integer(text, choice) || choice < 0 || choice > 2) return;
                mutate([choice](auto& svc, auto i) {
                    return svc.setPlacementMirror(i, static_cast<PI::Mirror>(choice));
                });
            }
        );
        p->rows.back().kind    = RowKind::Choice;
        p->rows.back().choices = {tr("ui.mirror.none"), tr("ui.mirror.x"), tr("ui.mirror.z")};
        b.button("reset", tr("ui.transform_reset"), [mutate] {
            mutate([](auto& svc, auto i) { return svc.resetPlacementTransform(i); });
        });
        for (int axis = 0; axis < 3; ++axis) {
            b.input(
                "move" + std::to_string(axis),
                std::string{tr("ui.move")} + "XYZ"[axis],
                "0",
                [p, mutate, axis](std::string text) {
                    int n;
                    if (!integer(text, n)) {
                        p->message = tr("ui.integer");
                        return;
                    }
                    mutate([axis, n](auto& svc, auto i) {
                        return svc.movePlacement(i, axis == 0 ? n : 0, axis == 1 ? n : 0, axis == 2 ? n : 0);
                    });
                }
            );
            auto const* selected   = s->placement().findPlacement(*id);
            int         coordinate = selected ? (axis == 0   ? selected->origin.x
                                                 : axis == 1 ? selected->origin.y
                                                             : selected->origin.z)
                                              : 0;
            b.input(
                "origin" + std::to_string(axis),
                std::string{tr("ui.origin")} + "XYZ"[axis],
                std::to_string(coordinate),
                [p, id = *id, axis](std::string text) {
                    int n;
                    if (!integer(text, n)) {
                        p->message = tr("ui.integer");
                        return;
                    }
                    auto active = app::accessSession();
                    if (!active) return;
                    auto* item = active->placement().findPlacement(id);
                    if (!item) {
                        p->message = tr("ui.placement_missing");
                        return;
                    }
                    auto origin = item->origin;
                    if (axis == 0) origin.x = n;
                    else if (axis == 1) origin.y = n;
                    else origin.z = n;
                    auto result = active->placement().setPlacementOrigin(id, origin);
                    p->message  = result ? tr("ui.origin_updated") : result.error().describe();
                    if (result) {
                        app::refreshCurrentClientProjectionState(*active);
                        p->rows.clear();
                        projection(p);
                    }
                }
            );
        }
        b.button("delete", tr("ui.delete"), [p, id = *id] {
            confirm(p, tr("ui.delete_confirm"), [p, id] {
                auto session = app::accessSession();
                if (!session) return;
                auto result = session->placement().removePlacement(id);
                p->message  = result ? tr("ui.deleted") : result.error().describe();
                if (result) app::refreshCurrentClientProjectionState(*session);
                p->rows.clear();
                projection(p);
            });
        });
    }
    b.button("clear_all", tr("ui.clear"), [p] {
        confirm(p, tr("ui.clear_confirm"), [p] {
            auto active = app::accessSession();
            if (!active) return;
            active->loads().cancelAll();
            active->placement().clearPlacements();
            app::refreshCurrentClientProjectionState(*active);
            p->message = tr("ui.cleared");
            p->rows.clear();
            projection(p);
        });
    });
    auto range = s->view().currentLayerRange();
    b.input("layers", tr("ui.layers"), range.enabled ? "true" : "false", [p](std::string) {
        auto session = app::accessSession();
        if (!session) return;
        if (session->view().currentLayerRange().enabled) (void)session->view().disableLayerRange();
        else (void)session->view().enableLayerRange();
        app::refreshCurrentClientProjectionState(*session);
        p->rows.clear();
        projection(p);
    });
    p->rows.back().kind = RowKind::Toggle;
    auto submitLayer    = [p](bool minimum, std::string text) {
        auto active = app::accessSession();
        auto client = ll::service::getClientInstance();
        if (!active || !client || !client->getLocalPlayer()) return;
        auto const& heights = client->getLocalPlayer()->getDimension().mHeightRange.get();
        int         bottom  = heights.mMin;
        int         top     = static_cast<int>(heights.mMax) - 1;
        if (bottom > top) return;
        auto current = active->view().currentLayerRange();
        
        int low  = std::clamp(current.minY, bottom, top);
        int high = std::clamp(current.maxY, low, top);
        if (minimum) low = clampLayerInput(text, low, bottom, high);
        else high = clampLayerInput(text, high, low, top);
        auto result = active->view().setRange(low, high);
        p->message  = result ? tr("ui.layers_updated") : result.error().describe();
        if (result) app::refreshCurrentClientProjectionState(*active);
        for (auto& row : p->rows)
            if (row.id == "layer_range") {
                row.value  = std::to_string(low);
                row.value2 = std::to_string(high);
            }
    };
    b.input("layer_range", tr("ui.layer_range"), std::to_string(range.minY), [submitLayer](std::string text) {
        submitLayer(true, std::move(text));
    });
    p->rows.back().kind    = RowKind::Range;
    p->rows.back().value2  = std::to_string(range.maxY);
    p->rows.back().submit2 = [submitLayer](std::string text) { submitLayer(false, std::move(text)); };
    moveActionsToFooter(*p, {"clear_all"});
}
void settings(std::shared_ptr<Page> const& p, std::shared_ptr<J> const& draft) {
    p->rows.clear();
    p->title = tr("ui.tab.settings");
    PageBuilder b{p};
    b.button("hud_layout", "HUD 布局 / HUD layout", [] { requestOpen(PageId::HudLayout); });
    b.button(
         "apply",
         tr("ui.apply"),
         [p, draft] {
             app::Config config;
             auto        decoded = ll::reflection::deserialize(config, *draft);
             if (!decoded) {
                 p->message = tr("ui.invalid_config");
                 return;
             }
             auto error = app::configStore().submit(config);
             if (!error.empty()) {
                 p->message = error;
                 return;
             }
             error      = app::configStore().save();
             p->dirty   = !error.empty();
             p->message = error.empty() ? tr("ui.config_saved") : tr("ui.config_save_failed") + error;
         }
    )
        .button(
            "defaults",
            tr("ui.defaults"),
            [p, draft] {
                *draft   = configJson(app::Config{});
                p->dirty = true;
                settings(p, draft);
            }
        )
        .button("reload", tr("ui.reload"), [p, draft] {
            confirm(p, tr("ui.reload_confirm"), [p, draft] {
                *draft   = configJson(app::configStore().snapshot());
                p->dirty = false;
                settings(p, draft);
            });
        });
    auto flat = draft->flatten();
    b.input(
        "hud_position",
        tr("ui.title.hud_position"),
        (*draft)["hud"]["position"] == "bottom_right" ? "1" : "0",
        [p, draft](std::string text) {
            if (text != "0" && text != "1") return;
            (*draft)["hud"]["position"] = text == "1" ? "bottom_right" : "bottom_left";
            p->dirty                    = true;
            p->message                  = tr("ui.draft");
            for (auto& row : p->rows)
                if (row.id == "hud_position") row.value = text;
        }
    );
    p->rows.back().kind    = RowKind::Choice;
    p->rows.back().choices = {tr("ui.hud.left"), tr("ui.hud.right")};
    for (auto const& [path, value] : flat.items()) {
        if (path == "/version" || path.starts_with("/hud/") || path.starts_with("/selection/faceColor/")
            || path.starts_with("/hull/blockColor/") || path.starts_with("/hull/propertyColor/"))
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
                    p->message                      = tr("ui.draft");
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
    for (auto const& [path, title] : {
             std::pair{"/selection/faceColor", "ui.title.selection_color"},
             {"/hull/blockColor",     "ui.title.block_color"    },
             {"/hull/propertyColor",  "ui.title.property_color" }
    }) {
        auto color = (*draft)[J::json_pointer(path)].get<std::array<double, 4>>();
        b.input(path, tr(title), formatColor(color), [p, draft, path = std::string(path)](std::string text) {
            auto color = parseColor(std::move(text));
            if (!color) {
                p->message = tr("ui.invalid_color");
                return;
            }
            (*draft)[J::json_pointer(path)] = *color;
            p->dirty                        = true;
            p->message                      = tr("ui.draft");
            for (auto& row : p->rows)
                if (row.id == path) {
                    row.color = *color;
                    row.value = formatColor(*color);
                }
        });
        auto& row       = p->rows.back();
        row.kind        = RowKind::Color;
        row.color       = color;
        row.colorSubmit = [p, draft, path = std::string(path)](int channel, double value) {
            if (channel < 0 || channel > 3 || !std::isfinite(value)) return;
            (*draft)[J::json_pointer(path)][channel] = std::clamp(value, 0., 1.);
            p->dirty                                 = true;
            p->message                               = tr("ui.draft");
            for (auto& row : p->rows)
                if (row.id == path) {
                    row.color[channel] = std::clamp(value, 0., 1.);
                    row.value          = formatColor(row.color);
                }
        };
    }
    moveActionsToFooter(*p, {"apply", "defaults", "reload"});
    
    auto published = std::make_shared<J>(configJson(app::configStore().snapshot()));
    auto valid = std::make_shared<bool>(true);
    p->refresh = [p, draft, published, valid] {
        if (*draft == *published) { *valid = true; return; }
        app::Config config;
        auto decoded = ll::reflection::deserialize(config, *draft);
        auto error = decoded ? app::configStore().submit(config) : tr("ui.invalid_config");
        *valid = error.empty();
        if (!*valid) { p->message = error; return; }
        *published = *draft;
        p->dirty = true;
        p->message = "已生效，离页保存 / Active; saved on leaving";
    };
    p->save = [p, valid] {
        if (p->refresh) p->refresh();
        if (!*valid) return false;
        if (!p->dirty) return true;
        auto error = app::configStore().save();
        if (!error.empty()) { p->message = tr("ui.config_save_failed") + error; return false; }
        p->dirty = false;
        return true;
    };
    for (auto& row : p->rows) {
        if (row.kind == RowKind::ActionBar)
            std::erase_if(row.actions, [](Row const& a) { return a.id == "apply"; });
        if (row.id.starts_with('/')) {
            row.reset = [p, draft, path = row.id] {
                auto defaults = configJson(app::Config{});
                (*draft)[J::json_pointer(path)] = defaults[J::json_pointer(path)];
                settings(p, draft);
                p->dirty = true;
            };
        }
    }
}
void keys(std::shared_ptr<Page> const& p) {
    p->title = tr("ui.tab.keys");
    for (auto const& spec : input::actionSpecs()) {
        if(spec.id==input::ActionId::InfoRelease||spec.id==input::ActionId::EasyPlaceRelease)continue;
        auto action = bindingActionName(spec.id);
        PageBuilder{p}
            .input(action, std::string(spec.label), describeBinding(bindingKeys(spec.id)), [p, action](std::string) {
                p->capturing = true;
                p->message    = "按住组合键，松开保存；Esc 取消 / Hold keys, release to save; Esc cancels";
                auto captured = std::make_shared<std::vector<int>>();
                p->rawKey     = [p, action, captured](int key, int type, bool down) {
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
                    p->capturing = false;
                    p->message   = tr("ui.key_saved");
                    p->rows.clear();
                    keys(p);
                };
            });
        auto& row = p->rows.back();
        row.kind  = RowKind::KeyBinding;
        row.clear = [p, action] {
            auto c = ll::service::getClientInstance();
            if (!c) return;
            auto error = saveBinding(*c, action, {});
            p->message = error.empty() ? tr("ui.key_cleared") : error;
            p->rows.clear();
            keys(p);
        };
        row.reset = [p, action, defaults = spec.keys] {
            auto c = ll::service::getClientInstance();
            if (!c) return;
            auto error = saveBinding(*c, action, defaults);
            p->message = error.empty() ? tr("ui.key_restored") : error;
            p->rows.clear();
            keys(p);
        };
    }
}
void selectionPage(std::shared_ptr<Page> const& p) {
    p->title = "选区编辑器 / Selection editor";
    auto s   = app::accessSession();
    if (!s) return;
    PageBuilder b{p};
    appendSelectionOrganization(p);
    for (int corner = 0; corner < 2; ++corner)
        for (int axis = 0; axis < 3; ++axis) {
            auto value      = corner ? s->selection().state().corner2 : s->selection().state().corner1;
            int  coordinate = value ? (axis == 0 ? value->x : axis == 1 ? value->y : value->z) : 0;
            b.input(
                "corner" + std::to_string(corner) + std::to_string(axis),
                localizedText("角点 / Corner") + " " + std::to_string(corner + 1) + " " + "XYZ"[axis],
                std::to_string(coordinate),
                [p, corner, axis](std::string text) {
                    int n;
                    if (!integer(text, n)) {
                        p->message = tr("ui.integer");
                        return;
                    }
                    auto session = app::accessSession();
                    if (!session) return;
                    auto previous =
                        corner ? session->selection().state().corner2 : session->selection().state().corner1;
                    BlockPos pos = previous.value_or(BlockPos{0, 0, 0});
                    if (axis == 0) pos.x = n;
                    else if (axis == 1) pos.y = n;
                    else pos.z = n;
                    if (corner) session->selection().setSelectionCorner2(pos);
                    else session->selection().setSelectionCorner1(pos);
                    p->message = tr("ui.updated");
                }
            );
        }
    b.button(
        "submode",
        s->selection().state().cornerMode == selection::tool::CornerMode::Corners ? "Corners → Expand"
                                                                                  : "Expand → Corners",
        [p] {
            auto session = app::accessSession();
            if (!session) return;
            session->selection().cycleCornerMode();
            p->rows.clear();
            selectionPage(p);
        }
    );
    b.button("clear_selection", "清空选区 / Clear selection", [p] {
        auto session = app::accessSession();
        if (!session) return;
        session->selection().clearSelection();
        p->rows.clear();
        selectionPage(p);
    });
    b.button("main_menu", "主菜单 / Main menu", [] { requestOpen(PageId::MainMenu); });
}
void mainMenu(std::shared_ptr<Page> const& p) {
    p->title = "主菜单 / Main menu";
    PageBuilder b{p};
    for (auto const& [id, label] : {
             std::pair{PageId::Projection,      "放置列表 / Placements"          },
             {PageId::Settings,        "配置菜单 / Configuration"         },
             {PageId::Resources,       "已加载结构 / Loaded schematics"   },
             {PageId::Placement,       "当前放置 / Selected placement"  },
             {PageId::Load,            "加载结构 / Load schematics"      },
             {PageId::Selection,       "选区编辑器 / Selection editor"  },
             {PageId::SaveSelection,   "保存选区 / Save selection"      },
             {PageId::Layers,          "分层渲染 / Render layers"       },
             {PageId::Verifier,        "校验器 / Verifier"              },
             {PageId::Materials,       "材料清单 / Materials"           },
             {PageId::Survival,        "生存辅助 / Survival assistance" },
             {PageId::Session,         "会话恢复 / Session recovery"    },
             {PageId::Edit,            "编辑投影 / Edit schematic"      },
             {PageId::WorldOperations, "创造世界操作 / World operations"},
             {PageId::Projects,        "建筑项目 / Projects"            },
             {PageId::Signs,           "告示牌文本 / Sign text"         },
             {PageId::Tasks,           "任务管理 / Task manager"        },
             {PageId::KeyBindings,     "按键绑定 / Key bindings"        }
    })
        b.button("navigate" + std::to_string(static_cast<int>(id)), label, [id] { requestOpen(id); });
    b.button("tool_toggle", "工具开关 / Toggle tool", [p] {
        auto s = app::accessSession();
        if (!s) return;
        s->tools().toggle();
        p->message = s->tools().state().enabled ? "工具已开启 / Tool ON" : "工具已关闭 / Tool OFF";
    });
    b.button("tool_mode", "工具模式 / Tool mode", [p] {
        auto s = app::accessSession();
        if (s) { s->tools().cycle(); p->message = std::to_string(static_cast<int>(s->tools().state().activeTool)); }
    });
    p->refresh = [weak = std::weak_ptr<Page>(p)] {
        auto page = weak.lock(); auto s = app::accessSession();
        if (!page || !s) return;
        constexpr char const* names[] = {"None / 无", "Placement / 放置", "Selection / 选区", "Edit / 编辑", "Paste / 粘贴",
            "Grid paste / 网格粘贴", "Fill / 填充", "Replace / 替换", "Delete / 删除", "Clone / 复制", "Move / 移动"};
        for (auto& row : page->rows) {
            if (row.id == "tool_mode") row.label = localizedText("工具模式 / Tool mode") + ": "
                + localizedText(names[static_cast<int>(s->tools().state().activeTool)]);
            if (row.id == "tool_toggle") row.label = localizedText("工具 / Tool") + ": "
                + tr(s->tools().state().enabled ? "ui.state_on" : "ui.state_off");
            if (row.id == "navigate" + std::to_string(static_cast<int>(PageId::Placement))) row.enabled = s->placement().selectedPlacementId().has_value();
        }
    };
    p->refresh();
}

} 
std::shared_ptr<Page> buildPage(PageId id) {
    auto page = std::make_shared<Page>();
    page->route = id;
    switch (id) {
    case PageId::HudLayout:
        page->title = "HUD 布局 / HUD layout";
        break;
    case PageId::Load:
        projection(page);
        break;
    case PageId::Tasks: {
        page->title = "任务管理 / Task manager";
        PageBuilder b{page};
        b.button("refresh", "刷新 / Refresh", [] { requestOpen(PageId::Tasks); });
        if (auto session = app::accessSession()) {
            auto save = session->saves().progress();
            b.info("save_progress", localizedText("结构保存 / Schematic save") + ": " + save.phase + " "
                + std::to_string(save.completed) + "/" + std::to_string(save.total) + " " + save.detail);
            b.button("save_details", "查看保存任务 / Save task details", [] { requestOpen(PageId::SaveSelection); });
            auto world = session->worldEdit().progress();
            b.info("world_progress", localizedText("世界操作 / World operation") + ": " + worldedit::phaseName(world.phase)
                + " " + world.detail);
            b.button("world_details", "查看世界任务 / World task details", [] { requestOpen(PageId::WorldOperations); });
            b.info("project_progress", session->projects().view().detail);
            b.button("project_details", "查看项目任务 / Project task details", [] { requestOpen(PageId::Projects); });
            auto jobs = session->loads().statuses(session->sessionGeneration());
            if (jobs.empty()) b.info("empty", "没有加载任务 / No loading tasks");
            for (auto const& job : jobs) {
                auto key = "task:" + std::to_string(job.id);
                b.info(key, std::to_string(job.id) + " " + tr("ui.phase." + job.phase) + " " + job.detail);
                if (!job.terminal) b.button(key + ":cancel", "取消 / Cancel", [task = job.id] {
                    if (auto s = app::accessSession()) (void)s->loads().cancel(task, s->sessionGeneration());
                    requestOpen(PageId::Tasks);
                });
            }
        }
        break;
    }
    case PageId::WorldOperations:
    case PageId::Projects:
    case PageId::Signs:
        populateAdvancedPage(page, id);
        break;
    case PageId::Resources:
    case PageId::Edit:
        populateOrganizationPage(page,id);
        break;
    case PageId::Materials:
    case PageId::Survival:
        populateSurvivalPage(page, id);
        break;
    case PageId::SaveSelection:
    case PageId::Layers:
    case PageId::Verifier:
    case PageId::Session:
        populateCorePage(page,id);
        break;
    case PageId::Placement:
        page->placementSettings = true;
        projection(page);
        break;
    case PageId::Projection:
        page->title = "结构放置 / Schematic placements";
        if (auto s = app::accessSession()) {
            PageBuilder b{page};
            b.button("load", "加载结构 / Load schematics", [] { requestOpen(PageId::Load); });
            for (auto const& ref : s->placement().managedPlacements()) {
                auto const& item = ref.get();
                auto key = "placement:" + std::to_string(item.id);
                b.button(key, item.name, [id = item.id] {
                    if (auto s = app::accessSession()) (void)s->placement().selectPlacement(id);
                    requestOpen(PageId::Placement);
                });
                page->rows.back().selected = s->placement().selectedPlacementId() == item.id;
                page->rows.back().kind = RowKind::TableRow;
                page->rows.back().columns = {item.name, item.enabled ? localizedText("启用 / Enabled") : localizedText("停用 / Disabled"),
                    std::to_string(item.origin.x) + ", " + std::to_string(item.origin.y) + ", " + std::to_string(item.origin.z)};
            }
            if (s->placement().managedPlacements().empty()) b.info("empty", "没有放置 / No placements");
        }
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
        selectionPage(page);
        break;
    case PageId::KeyBindings:
        keys(page);
        break;
    }
    if (id == PageId::Resources) {
        std::vector<Row> grouped;
        for (auto& row : page->rows) {
            if (!row.id.starts_with("resource:")) { grouped.push_back(std::move(row)); continue; }
            auto end = row.id.find(':', 9);
            auto key = row.id.substr(0, end);
            if (row.kind == RowKind::Info) {
                auto split = row.label.find('\n');
                Row entry;
                entry.id = key; entry.label = row.label.substr(0, split);
                entry.value = split == row.label.npos ? "" : row.label.substr(split + 1);
                std::replace(entry.value.begin(), entry.value.end(), '\n', ' ');
                entry.kind = RowKind::Entry; entry.submit = [](std::string) {};
                grouped.push_back(std::move(entry));
            } else if (!grouped.empty() && grouped.back().id == key) grouped.back().actions.push_back(std::move(row));
        }
        page->rows = std::move(grouped);
    }
    return page;
}
} 
