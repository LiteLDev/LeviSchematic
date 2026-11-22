#include "levischematic/presentation/ui/pages/advanced/AdvancedPages.h"
#include "levischematic/app/lifecycle/AppKernel.h"
#include "levischematic/presentation/hud/ProjectionHud.h"
#include "levischematic/presentation/ui/i18n/UiI18n.h"
#include "levischematic/presentation/ui/pages/common/PageCommon.h"
#include "levischematic/shared/i18n/I18n.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include <charconv>
namespace levischematic::ui {
namespace {
std::string operationName(worldedit::Operation op) {
    return tr(
        app::toolNameKey(
            static_cast<editor::ToolMode>(static_cast<int>(editor::ToolMode::Paste) + static_cast<int>(op))
        )
    );
}
bool result(std::shared_ptr<Page> const& p, app::StatusResult<std::string> const& r) {
    p->message = r ? tr("ui.advanced.done") : r.error();
    return static_cast<bool>(r);
}
void world(std::shared_ptr<Page> const& p) {
    p->title = tr("ui.advanced.world_operations");
    PageBuilder b{p};
    auto        s = app::accessSession();
    if (!s) return;
    auto options = s->worldEdit().options();
    b.info("world_warning", tr("ui.advanced.actual_world_changes_cancel_keeps"));
    b.info("interaction_help", tr("ui.advanced.middle_tool_click_samples_primary"));
    for (auto [name, secondary] : {
             std::pair{"primary",   false},
             {"secondary", true }
    })
        b.button(
            name,
            secondary ? tr("ui.advanced.sample_source_at_aim") : tr("ui.advanced.sample_target_at_aim"),
            [secondary] {
                auto s = app::accessSession();
                auto c = ll::service::getClientInstance();
                if (s && c) s->worldEdit().sample(*c, secondary);
            }
        );
    b.button("operation", tr("ui.advanced.operation", operationName(options.operation)), [p] {
        auto s = app::accessSession();
        if (!s) return;
        if (!s->worldEdit().progress().terminal()) {
            p->message = "Cancel current task before changing operation.";
            return;
        }
        auto n                             = (static_cast<int>(s->worldEdit().options().operation) + 1) % 7;
        s->worldEdit().options().operation = static_cast<worldedit::Operation>(n);
        s->tools().setMode(static_cast<editor::ToolMode>(static_cast<int>(editor::ToolMode::Paste) + n));
        requestOpen(PageId::WorldOperations);
    });
    b.input(
        "policy",
        tr("ui.advanced.replacement"),
        std::to_string(static_cast<int>(options.policy)),
        [](std::string text) {
            int n;
            auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), n);
            if (ec != std::errc{} || end != text.data() + text.size() || n < 0 || n > 2) return;
            auto s = app::accessSession();
            if (s) s->worldEdit().options().policy = static_cast<worldedit::ReplacePolicy>(n);
        }
    );
    p->rows.back().kind = RowKind::Choice;
    p->rows.back()
        .choices = {tr("ui.advanced.target_air"), tr("ui.advanced.source_non_air"), tr("ui.advanced.all_valid_cells")};
    for (int i = 0; i < 3; ++i)
        b.input(
            "offset" + std::to_string(i),
            tr("ui.advanced.clone_move_offset") + " " + "XYZ"[i],
            std::to_string(options.offset[i]),
            [p, i](std::string value) {
                int n;
                auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), n);
                if (ec != std::errc{} || end != value.data() + value.size()) {
                    p->message = "Invalid coordinate.";
                    return;
                }
                auto s = app::accessSession();
                if (s) s->worldEdit().options().offset[i] = n;
            }
        );
    b.button("match_type", options.matchTypeOnly ? tr("ui.advanced.match_type") : tr("ui.advanced.match_state"), [] {
        auto s = app::accessSession();
        if (s) s->worldEdit().options().matchTypeOnly = !s->worldEdit().options().matchTypeOnly;
        requestOpen(PageId::WorldOperations);
    });
    b.button("layer", options.layerOnly ? tr("ui.advanced.current_layers") : tr("ui.advanced.full_valid_area"), [] {
        auto s = app::accessSession();
        if (s) s->worldEdit().options().layerOnly = !s->worldEdit().options().layerOnly;
        requestOpen(PageId::WorldOperations);
    });
    b.button("prepare", tr("ui.advanced.prepare"), [p] {
        auto s = app::accessSession();
        auto c = ll::service::getClientInstance();
        if (!s || !c) return;
        auto r     = s->worldEdit().prepare(*c, s->sessionGeneration());
        p->message = r ? "Preflight queued." : r.error();
    });
    b.button("start", tr("ui.advanced.start_prepared_task"), [] {
        auto s = app::accessSession();
        if (s) s->worldEdit().start();
    });
    b.button("pause", tr("ui.advanced.pause"), [] {
        auto s = app::accessSession();
        if (s) s->worldEdit().pause();
    });
    b.button("resume", tr("ui.advanced.resume"), [] {
        auto s = app::accessSession();
        if (s) s->worldEdit().resume();
    });
    b.button("cancel", tr("ui.advanced.cancel"), [] {
        auto s = app::accessSession();
        if (s) s->worldEdit().cancel();
    });
    b.info("progress", "");
    p->refresh = [p] {
        auto s = app::accessSession();
        if (!s) return;
        auto q = s->worldEdit().progress();
        for (auto& row : p->rows)
            if (row.id == "progress")
                row.label = tr(worldedit::phaseKey(q.phase)) + " #" + std::to_string(q.id)
                          + " prepared=" + std::to_string(q.prepared) + " observed=" + std::to_string(q.observed) + "/"
                          + std::to_string(q.total) + " verified=" + std::to_string(q.verified) + " skipped="
                          + std::to_string(q.skipped) + " rejected=" + std::to_string(q.rejected) + "\n" + q.detail;
    };
    p->refresh();
    moveActionsToFooter(*p, {"prepare", "start", "cancel"});
}
void projects(std::shared_ptr<Page> const& p) {
    p->title = tr("ui.advanced.projects");
    PageBuilder b{p};
    auto        s = app::accessSession();
    if (!s) return;
    auto view = s->projects().view();
    auto name = std::make_shared<std::string>(view.name), version = std::make_shared<std::string>("Version"),
         description = std::make_shared<std::string>();
    b.input("project_name", tr("ui.advanced.project_name"), *name, [name](std::string value) {
        *name = std::move(value);
    });
    b.button("create", tr("ui.advanced.create_at_player"), [p, name] {
        auto s = app::accessSession();
        auto c = ll::service::getClientInstance();
        if (s && c && c->getLocalPlayer())
            result(p, s->projects().create(*name, BlockPos(c->getLocalPlayer()->getPosition())));
    });
    b.button("open", tr("ui.advanced.open"), [p, name] {
        auto s = app::accessSession();
        if (s) {
            if (result(p, s->projects().open(*name))) requestOpen(PageId::Projects);
        }
    });
    b.button("rename", tr("ui.advanced.rename"), [p, name] {
        auto s = app::accessSession();
        if (s) result(p, s->projects().rename(*name));
    });
    for (auto const& name : s->projects().list())
        b.button("open:" + name, name, [p, name] {
            auto s = app::accessSession();
            if (s) {
                result(p, s->projects().open(name));
                requestOpen(PageId::Projects);
            }
        });
    if (view.open) {
        b.info(
            "project_info",
            view.name + " current=" + std::to_string(view.current) + " last pasted=" + std::to_string(view.lastPasted)
        );
        b.button("project_origin", tr("ui.advanced.origin_at_player"), [p] {
            auto s = app::accessSession();
            auto c = ll::service::getClientInstance();
            if (s && c && c->getLocalPlayer())
                result(p, s->projects().setOrigin(BlockPos(c->getLocalPlayer()->getPosition())));
        });
        b.input("version_name", tr("ui.advanced.version_name"), *version, [version](std::string v) {
            *version = std::move(v);
        });
        b.input("description", tr("ui.advanced.description"), *description, [description](std::string v) {
            *description = std::move(v);
        });
        b.button("save_version", tr("ui.advanced.snapshot_world_selection"), [p, version, description] {
            auto s = app::accessSession();
            if (s) result(p, s->projects().saveVersion(s->projectServices(), *version, *description));
        });
        b.input("work", tr("ui.advanced.work_note"), view.work, [p](std::string text) {
            auto s = app::accessSession();
            if (s) result(p, s->projects().setWork(std::move(text)));
        });
        for (auto const& v : view.versions)
            b.button(
                "version:" + std::to_string(v.id),
                std::to_string(v.id) + " " + v.name + " " + v.description,
                [p, id = v.id] {
                    auto s = app::accessSession();
                    if (s) result(p, s->projects().checkout(s->projectServices(), id));
                }
            );
        b.button("paste_version", tr("ui.advanced.prepare_version_paste"), [p] {
            auto s = app::accessSession();
            auto c = ll::service::getClientInstance();
            if (s && c) result(p, s->projects().paste(s->projectServices(), *c));
        });
        b.button("restore_selection", tr("ui.advanced.restore_version_selection"), [p] {
            auto s = app::accessSession();
            if (s) result(p, s->projects().restoreSelection(s->projectServices()));
        });
        for (auto [name, direction] : {
                 std::pair{"previous", -1},
                 {"next",     1 }
        })
            b.button(
                name,
                direction < 0 ? tr("ui.advanced.previous_version") : tr("ui.advanced.next_version"),
                [p, direction] {
                    auto s = app::accessSession();
                    if (!s) return;
                    auto view = s->projects().view();
                    result(p, s->projects().checkout(s->projectServices(), view.current + direction));
                }
            );
        b.button("project_cancel", tr("ui.advanced.cancel_project_task"), [] {
            auto s = app::accessSession();
            if (s) s->projects().cancel(s->projectServices());
        });
    }
    b.button("refresh", tr("ui.advanced.refresh"), [] { requestOpen(PageId::Projects); });
    b.info("project_status", view.detail);
    p->refresh = [p] {
        auto s = app::accessSession();
        if (s)
            for (auto& row : p->rows)
                if (row.id == "project_status") row.label = s->projects().view().detail;
    };
}
void signs(std::shared_ptr<Page> const& p) {
    p->title = tr("ui.advanced.sign_text");
    PageBuilder b{p};
    for (auto [id, back] : {
             std::pair{"front", false},
             {"back",  true }
    })
        b.button(id, back ? tr("ui.advanced.copy_back") : tr("ui.advanced.copy_front"), [p, back] {
            auto s = app::accessSession();
            auto c = ll::service::getClientInstance();
            if (s && c) p->message = s->signs().copy(*c, s->placement(), back);
        });
    b.button("paste", tr("ui.advanced.paste_into_next_opened_sign"), [p] {
        auto s = app::accessSession();
        if (s) p->message = s->signs().arm();
    });
    b.button("cancel_paste", tr("ui.advanced.cancel_pending_paste"), [] {
        auto s = app::accessSession();
        if (s) s->signs().cancel();
    });
    auto s = app::accessSession();
    if (s) b.info("clipboard", s->signs().text());
}
} // namespace
void populateAdvancedPage(std::shared_ptr<Page> const& p, PageId id) {
    p->layout     = PageLayout::List;
    p->searchable = false;
    if (id == PageId::WorldOperations) world(p);
    else if (id == PageId::Projects) projects(p);
    else signs(p);
    PageBuilder b{p};
    mainMenuButton(b);
}
} // namespace levischematic::ui
