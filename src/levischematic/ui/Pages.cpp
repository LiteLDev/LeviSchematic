#include "Bindings.h"
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
        {"/visibility/renderDistanceBlocks",    tr("ui.field.distance")   },
        {"/visibility/frustumCulling",          tr("ui.field.frustum")    },
        {"/projection/alpha",                   tr("ui.field.alpha")      },
        {"/projection/texture",                 tr("ui.field.texture")    },
        {"/hull/expansion",                     tr("ui.field.expansion")  },
        {"/hull/depthBias",                     tr("ui.field.depth")      },
        {"/hull/slopeBias",                     tr("ui.field.slope")      },
        {"/performance/tickBudgetMs",           tr("ui.field.tick")       },
        {"/performance/frameBudgetMs",          tr("ui.field.frame")      },
        {"/performance/maxLoadRequests",        tr("ui.field.loads")      },
        {"/performance/geometryBuildsPerFrame", tr("ui.field.builds")     },
        {"/performance/cameraResortsPerFrame",  tr("ui.field.resorts")    },
        {"/performance/queuedChunkBuilds",      tr("ui.field.queue")      },
        {"/diagnostics",                        tr("ui.field.diagnostics")},
    };
    if (auto it = labels.find(path); it != labels.end()) return it->second;
    for (auto const& [prefix, label] : {
             std::pair{"/selection/faceColor/", tr("ui.field.selection_color")},
             {"/hull/blockColor/",     tr("ui.field.block_color")    },
             {"/hull/propertyColor/",  tr("ui.field.property_color") }
    }) {
        if (path.starts_with(prefix) && path.back() >= '0' && path.back() <= '3')
            return std::string(label) + " / " + "RGBA"[path.back() - '0'] + tr("ui.unit_color");
    }
    return path;
}
bool integer(std::string const& text, int& value) {
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}
void confirm(std::shared_ptr<Page> const& p, std::string text, std::function<void()> action) {
    p->rows.clear();
    p->message = std::move(text);
    PageBuilder{p}
        .button(
            "yes",
            tr("ui.confirm"),
            [p, action = std::move(action)] {
                action();
                p->refresh = {};
            }
        )
        .button("cancel", tr("ui.cancel"), [p] {
            p->rows.clear();
            p->message = tr("ui.cancelled_back");
        });
}
void projection(std::shared_ptr<Page> const& p) {
    auto s = app::accessSession();
    if (!s) {
        p->message = tr("ui.no_session");
        return;
    }
    p->title = tr("ui.tab.projection");
    PageBuilder b{p};
    
    
    b.button("refresh", tr("ui.refresh"), [p] {
        p->rows.clear();
        projection(p);
    });
    for (auto const& file : s->runtime().listAvailableSchematics()) {
        b.button("file:" + file, tr("ui.load") + file, [p, file] {
            auto session = app::accessSession();
            auto client  = ll::service::getClientInstance();
            if (!session || !client || !client->getLocalPlayer()) return;
            auto* player = client->getLocalPlayer();
            auto  result = session->loads().request(
                session->runtime().resolveSchematicPath(file),
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
        });
    }
    for (auto const& ref : s->placement().orderedPlacements()) {
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
    auto id = s->placement().selectedPlacementId();
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
        };
        b.button(
             "enabled",
             tr("ui.enabled"),
             [mutate] { mutate([](auto& svc, auto i) { return svc.togglePlacementEnabled(i); }); }
        )
            .button(
                "render",
                tr("ui.render"),
                [mutate] { mutate([](auto& svc, auto i) { return svc.togglePlacementRender(i); }); }
            )
            .button("rotate", tr("ui.rotate"), [mutate] {
                mutate([](auto& svc, auto i) { return svc.rotatePlacement(i, PI::Rotation::CW_90); });
            });
        for (auto [name, mirror] : {
                 std::pair{tr("ui.mirror.none"), PI::Mirror::NONE    },
                 {tr("ui.mirror.x"),    PI::Mirror::MIRROR_X},
                 {tr("ui.mirror.z"),    PI::Mirror::MIRROR_Z}
        })
            b.button("mirror" + std::to_string(static_cast<int>(mirror)), name, [mutate, mirror] {
                mutate([mirror](auto& svc, auto i) { return svc.setPlacementMirror(i, mirror); });
            });
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
                    if (result) app::refreshCurrentClientProjectionState(*active);
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
    b.button("layers", std::string{tr("ui.layers")} + (range.enabled ? tr("ui.on") : tr("ui.off")), [p] {
        auto session = app::accessSession();
        if (!session) return;
        if (session->view().currentLayerRange().enabled) (void)session->view().disableLayerRange();
        else (void)session->view().enableLayerRange();
        app::refreshCurrentClientProjectionState(*session);
        p->rows.clear();
        projection(p);
    });
    for (bool minimum : {true, false})
        b.input(
            minimum ? "minY" : "maxY",
            minimum ? tr("ui.min_y") : tr("ui.max_y"),
            std::to_string(minimum ? range.minY : range.maxY),
            [p, minimum](std::string text) {
                int n;
                if (!integer(text, n)) {
                    p->message = tr("ui.integer");
                    return;
                }
                auto session = app::accessSession();
                if (!session) return;
                auto result = minimum ? session->view().setMinY(n) : session->view().setMaxY(n);
                p->message  = result ? tr("ui.layers_updated") : result.error().describe();
                if (result) app::refreshCurrentClientProjectionState(*session);
            }
        );
}
void settings(std::shared_ptr<Page> const& p, std::shared_ptr<J> const& draft) {
    p->rows.clear();
    p->title = tr("ui.tab.settings");
    PageBuilder b{p};
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
    for (auto const& [path, value] : flat.items()) {
        if (path == "/version") continue;
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
                    settings(p, draft);
                } catch (std::exception const& e) {
                    p->message = e.what();
                }
            }
        );
    }
}
void keys(std::shared_ptr<Page> const& p) {
    p->title    = tr("ui.tab.keys");
    auto client = ll::service::getClientInstance();
    if (!client) return;
    auto layout = client->getOptions().getCurrentKeyboardRemapping();
    if (!layout) return;
    auto names  = ll::input::KeyRegistry::getInstance().getRegisteredKeys();
    auto prefix = LeviSchematic::getInstance().getSelf().getName() + ".";
    for (auto const& name : names) {
        if (!name.starts_with(prefix)) continue;
        auto        action  = "key." + name;
        auto        display = name == prefix + "levischematic.ui.open"              ? tr("ui.key.open")
                            : name == prefix + "levischematic.view.y.max.increment" ? tr("ui.key.increase")
                            : name == prefix + "levischematic.view.y.max.decrement" ? tr("ui.key.decrease")
                                                                                    : name;
        auto const& mapping = layout->getKeymappingByAction(action);
        PageBuilder{p}.button(
            action,
            display + ": " + static_cast<RemappingLayout&>(*layout).getMappedKeyName(mapping),
            [p, action] {
                p->capturing = true;
                p->message   = tr("ui.capture");
                p->rawKey    = [p, action](int key, int type) {
                    if (type != 4 && type != 1) return;
                    auto c = ll::service::getClientInstance();
                    if (!c) return;
                    auto current = c->getOptions().getCurrentKeyboardRemapping();
                    if (!current) return;
                    int converted = current->_rawKeyToKey(key, static_cast<RawInputType>(type));
                    for (auto const& map : *current->mKeymappings) {
                        if (*map.mAction == action) continue;
                        for (int bound : *map.mKeys)
                            if (bound == converted) {
                                p->message = tr("ui.conflict") + *map.mAction + tr("ui.choose_again");
                                return;
                            }
                    }
                    auto error = saveBinding(*c, action, {converted});
                    if (!error.empty()) {
                        p->message = error;
                        return;
                    }
                    p->capturing = false;
                    p->message   = tr("ui.key_saved");
                    p->rows.clear();
                    keys(p);
                };
            }
        );
        PageBuilder{p}
            .button(
                "clear:" + action,
                tr("ui.key_clear") + display,
                [p, action] {
                    auto c = ll::service::getClientInstance();
                    if (!c) return;
                    auto error = saveBinding(*c, action, {});
                    p->message = error.empty() ? tr("ui.key_cleared") : error;
                    p->rows.clear();
                    keys(p);
                }
            )
            .button("default:" + action, tr("ui.key_default") + display, [p, action] {
                auto c = ll::service::getClientInstance();
                if (!c) return;
                auto current = c->getOptions().getCurrentKeyboardRemapping();
                if (!current) return;
                for (auto const& map : *current->mDefaultMappings)
                    if (*map.mAction == action) {
                        auto error = saveBinding(*c, action, *map.mKeys);
                        p->message = error.empty() ? tr("ui.key_restored") : error;
                        break;
                    }
                p->rows.clear();
                keys(p);
            });
    }
}
} 
std::shared_ptr<Page> buildPage(PageId id) {
    auto page = std::make_shared<Page>();
    switch (id) {
    case PageId::Projection:
        projection(page);
        break;
    case PageId::Settings: {
        auto draft = std::make_shared<J>(configJson(app::configStore().snapshot()));
        settings(page, draft);
        break;
    }
    case PageId::KeyBindings:
        keys(page);
        break;
    }
    return page;
}
} 
