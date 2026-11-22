#include "levischematic/presentation/ui/pages/core/CorePages.h"
#include "levischematic/app/lifecycle/AppKernel.h"
#include "levischematic/presentation/ui/pages/common/PageCommon.h"
#include "levischematic/schematic/io/SchematicFiles.h"
#include "levischematic/schematic/placement/view/ProjectionRefresh.h"
#include "levischematic/shared/i18n/I18n.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include <charconv>
#include <functional>
#include <tuple>

namespace levischematic::ui {
namespace {
void refresh(std::shared_ptr<Page> const& page, PageId id) {
    page->rows.clear();
    populateCorePage(page, id);
}
bool integer(std::string const& text, int& value) {
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}
void confirm(std::shared_ptr<Page> const& page, std::string message, PageId id, std::function<void()> action) {
    (void)id;
    beginModal(page, std::move(message));
    PageBuilder{page}
        .button(
            "confirm",
            tr("ui.core.confirm"),
            [page, action = std::move(action)] {
                dismissModal(page);
                action();
            }
        )
        .button("cancel", tr("ui.core.back"), [page] { dismissModal(page); });
}
std::string modeLabel(editor::LayerMode mode) {
    switch (mode) {
    case editor::LayerMode::All:
        return tr("ui.core.layer_mode.all");
    case editor::LayerMode::Single:
        return tr("ui.core.layer_mode.single");
    case editor::LayerMode::Below:
        return tr("ui.core.layer_mode.below");
    case editor::LayerMode::Above:
        return tr("ui.core.layer_mode.above");
    case editor::LayerMode::Range:
        return tr("ui.core.layer_mode.range");
    }
    return tr("ui.core.layer_mode.all");
}
void layers(std::shared_ptr<Page> const& p) {
    p->title  = tr("ui.core.configuration");
    p->layout = PageLayout::Config;
    auto s    = app::accessSession();
    if (!s) return;
    auto        range = s->view().currentLayerRange();
    PageBuilder b{p};
    b.input("mode", tr("ui.core.layer_mode"), std::to_string(static_cast<int>(range.mode)), [p](std::string) {
        auto s = app::accessSession();
        if (!s) return;
        s->view().cycleMode(1);
        app::refreshCurrentClientProjectionState(*s);
        refresh(p, PageId::Layers);
    });
    p->rows.back().kind    = RowKind::Choice;
    p->rows.back().choices = {
        modeLabel(editor::LayerMode::All),
        modeLabel(editor::LayerMode::Single),
        modeLabel(editor::LayerMode::Below),
        modeLabel(editor::LayerMode::Above)
    };
    p->rows.back().choices.push_back(modeLabel(editor::LayerMode::Range));
    p->rows.back().tooltip = tr("ui.core.layer_mode.tip");
    b.input("axis", tr("ui.core.axis"), std::to_string(static_cast<int>(range.axis)), [p](std::string text) {
        int n;
        if (!integer(text, n) || n < 0 || n > 2) return;
        auto s = app::accessSession();
        if (!s) return;
        auto range = s->view().currentLayerRange();
        range.axis = static_cast<editor::LayerAxis>(n);
        (void)s->view().configure(range);
        app::refreshCurrentClientProjectionState(*s);
        refresh(p, PageId::Layers);
    });
    p->rows.back().kind    = RowKind::Choice;
    p->rows.back().choices = {"X", "Y", "Z"};
    for (auto const& [id, label, value] : {
             std::tuple{"boundary", tr("ui.core.layer"),   range.boundary()},
             {"minimum",  tr("ui.core.minimum"), range.minY      },
             {"maximum",  tr("ui.core.maximum"), range.maxY      }
    }) {
        b.input(id, label, std::to_string(value), [p, id = std::string(id)](std::string text) {
            int n;
            if (!integer(text, n)) {
                p->message = tr("ui.core.integer_required");
                return;
            }
            auto s = app::accessSession();
            if (!s) return;
            auto range = s->view().currentLayerRange();
            if (id == "boundary") range.boundary() = n;
            else if (id == "minimum") range.minY = n;
            else range.maxY = n;
            auto result = s->view().configure(range);
            p->message  = result ? tr("ui.core.updated") : result.error().describe();
            app::refreshCurrentClientProjectionState(*s);
            refresh(p, PageId::Layers);
        });
    }
    b.input("follow", tr("ui.core.follow_player"), range.followPlayer ? "true" : "false", [p](std::string) {
        auto s = app::accessSession();
        if (!s) return;
        auto range         = s->view().currentLayerRange();
        range.followPlayer = !range.followPlayer;
        (void)s->view().configure(range);
        refresh(p, PageId::Layers);
    });
    p->rows.back().kind = RowKind::Toggle;
    for (bool minimum : {true, false}) {
        b.input(
            minimum ? "move_min" : "move_max",
            minimum ? tr("ui.core.move_minimum") : tr("ui.core.move_maximum"),
            (minimum ? range.moveMinimum : range.moveMaximum) ? "true" : "false",
            [p, minimum](std::string) {
                auto s = app::accessSession();
                if (!s) return;
                auto range = s->view().currentLayerRange();
                if (minimum) range.moveMinimum = !range.moveMinimum;
                else range.moveMaximum = !range.moveMaximum;
                (void)s->view().configure(range);
                refresh(p, PageId::Layers);
            }
        );
        p->rows.back().kind = RowKind::Toggle;
    }
    b.button("here", tr("ui.core.set_to_player_layer"), [p] {
        auto s      = app::accessSession();
        auto client = ll::service::getClientInstance();
        if (s && client && client->getLocalPlayer()) {
            s->view().setAt(BlockPos(client->getLocalPlayer()->getPosition()));
            app::refreshCurrentClientProjectionState(*s);
            refresh(p, PageId::Layers);
        }
    });
    b.info("help", tr("ui.core.layer_help"));
}
void save(std::shared_ptr<Page> const& p, std::shared_ptr<app::SaveOptions> options) {
    p->title      = tr("ui.core.save_schematic");
    p->layout     = PageLayout::List;
    p->searchable = false;
    auto s        = app::accessSession();
    if (!s) return;
    auto        progress = s->saves().progress();
    PageBuilder b{p};
    b.input("filename", tr("ui.core.filename"), p->fileName, [p](std::string text) {
        p->fileName = std::move(text);
        for (auto& row : p->rows)
            if (row.id == "filename") row.value = p->fileName;
    });
    b.input(
        "entities",
        tr("ui.core.include_entities"),
        options->includeEntities ? "true" : "false",
        [p, options](std::string) {
            options->includeEntities = !options->includeEntities;
            p->rows.clear();
            save(p, options);
        }
    );
    p->rows.back().kind = RowKind::Toggle;
    auto submit         = [p, options](bool overwrite) {
        auto s      = app::accessSession();
        auto client = ll::service::getClientInstance();
        if (!s || !client || !client->getLocalPlayer()) return;
        auto request      = *options;
        request.overwrite = overwrite;
        try {
            auto result = s->saves().request(
                s->selection().state(),
                s->runtime().schematicDirectory(),
                p->fileName,
                static_cast<int>(client->getLocalPlayer()->getDimensionId()),
                s->sessionGeneration(),
                request
            );
            p->message = result ? tr("ui.core.save_queued") : result.error();
        } catch (std::exception const& e) {
            p->message = e.what();
        }
        p->rows.clear();
        save(p, options);
    };
    auto confirmMerge = [p](std::function<void()> next) {
        auto s       = app::accessSession();
        auto regions = s && s->selection().state().normal ? s->selection().state().regions.size() : 0;
        if (regions > 1 && schematic::fileSuffix(pathFromUtf8(p->fileName)) == ".mcstructure")
            confirm(p, tr("save.notice.merged", regions), PageId::SaveSelection, std::move(next));
        else next();
    };
    b.button("save", tr("ui.core.save_new_file"), [submit, confirmMerge] { confirmMerge([submit] { submit(false); }); })
        .in(Slot::FooterLeft)
        .tip(tr("ui.core.mcstructure_the_extension_selects"));
    b.button("overwrite", tr("ui.core.overwrite"), [p, submit] {
         confirm(p, tr("ui.save.overwrite_confirm", p->fileName), PageId::SaveSelection, [submit] { submit(true); });
     }).in(Slot::FooterLeft);
    b.button("save_status", tr("ui.core.refresh_progress"), [p, options] {
         p->rows.clear();
         save(p, options);
     }).in(Slot::TopBar);
    b.button("save_cancel", tr("ui.core.cancel_task"), [p, options] {
         auto s = app::accessSession();
         if (s) s->saves().cancel();
         p->rows.clear();
         save(p, options);
     }).in(Slot::TopBar);
    navButton(b, "area_editor", tr("ui.core.area_editor"), PageId::Selection, Slot::FooterLeft, icons::MenuAreaEditor);
    mainMenuButton(b);
    b.info(
        "progress",
        progress.phase + " " + std::to_string(progress.completed) + "/" + std::to_string(progress.total)
    );
    b.info("detail", progress.detail);
    b.info("time_span", tr("ui.core.capture_spans_multiple_ticks"));
    p->refresh = [weak = std::weak_ptr<Page>(p)] {
        auto page = weak.lock();
        auto s    = app::accessSession();
        if (!page || !s) return;
        auto progress = s->saves().progress();
        for (auto& row : page->rows) {
            if (row.id == "progress")
                row.label =
                    progress.phase + " " + std::to_string(progress.completed) + "/" + std::to_string(progress.total);
            if (row.id == "detail") row.label = progress.detail;
        }
    };
}
void verifier(std::shared_ptr<Page> const& p) {
    p->title    = tr("ui.core.schematic_verifier");
    p->layout   = PageLayout::List;
    auto s      = app::accessSession();
    auto client = ll::service::getClientInstance();
    if (!s || !client || !client->getLocalPlayer()) return;
    auto*       player    = client->getLocalPlayer();
    int         dimension = static_cast<int>(player->getDimensionId());
    PageBuilder b{p};
    auto        selected = s->placement().requireSelectedPlacement();
    if (!selected || selected.value()->dimensionId != dimension) {
        b.info("no_selection", tr("ui.core.select_a_placement_in_this_dimension"));
    }
    for (auto const& ref : s->placement().orderedPlacements()) {
        auto const& value = ref.get();
        if (value.dimensionId != dimension) continue;
        b.button(
             "placement:" + std::to_string(value.id),
             value.name,
             [p, id = value.id] {
                 auto s = app::accessSession();
                 if (!s) return;
                 (void)s->placement().selectPlacement(id);
                 refresh(p, PageId::Verifier);
             }
        )
            .kind(RowKind::TableRow)
            .icon(icons::Litematic);
        b.last().selected  = selected && selected.value()->id == value.id;
        b.last().textColor = value.enabled ? colors::Green : colors::Red;
    }
    if (!selected || selected.value()->dimensionId != dimension) return;
    auto id = selected.value()->id;
    b.info("selected", tr("ui.core.placement") + selected.value()->name);
    for (auto const& [key, label, layersOnly] : {
             std::tuple{"start_all",    tr("ui.core.start_full"),   false},
             {"start_layers", tr("ui.core.start_layers"), true }
    })
        b.button(key, label, [p, id, layersOnly] {
             auto s = app::accessSession();
             if (!s) return;
             p->message =
                 s->verifier().startTask(id, layersOnly) ? tr("ui.core.started") : tr("ui.core.placement_disabled");
             refresh(p, PageId::Verifier);
         }).in(Slot::TopBar);
    b.button("stop", tr("ui.core.stop"), [p, id] {
         auto s = app::accessSession();
         if (s) s->verifier().stopTask(id);
         refresh(p, PageId::Verifier);
     }).in(Slot::TopBar);
    b.button("reset", tr("ui.core.reset"), [p, id] {
         auto s = app::accessSession();
         if (s) s->verifier().resetTask(id);
         refresh(p, PageId::Verifier);
     }).in(Slot::TopBar);
    b.button("refresh_report", tr("ui.core.refresh_report"), [p] { refresh(p, PageId::Verifier); }).in(Slot::TopRight);
    b.button("clear_ignored", tr("ui.core.clear_ignored_groups"), [p, id] {
         auto s = app::accessSession();
         if (s) s->verifier().clearIgnored(id);
         refresh(p, PageId::Verifier);
     }).in(Slot::FooterLeft);
    auto report = s->verifier().report(id, BlockPos(player->getPosition()));
    b.info(
        "task",
        std::string(report.running ? tr("ui.core.running") : tr("ui.core.stopped"))
            + (report.building ? tr("ui.core.building") : "") + (report.stale ? tr("ui.core.stale") : "")
            + std::to_string(report.scanned) + "/" + std::to_string(report.total)
    );
    for (size_t i = 0; i < report.counts.size(); ++i) {
        auto status = static_cast<verifier::VerificationStatus>(i);
        b.button(
            "count:" + std::to_string(i),
            tr(verifier::verificationStatusKey(status)) + ": " + std::to_string(report.counts[i]),
            [] {}
        );
        b.last().kind = RowKind::TableRow;
        if (i >= 2) {
            Row highlight;
            highlight.id      = "highlight:" + std::to_string(i);
            highlight.label   = tr("ui.core.toggle_highlight");
            highlight.tooltip = tr("ui.core.highlight_these_positions_in_the");
            highlight.submit  = [p, id, status](std::string) {
                auto s = app::accessSession();
                if (s) s->verifier().toggleHighlight(id, status);
                refresh(p, PageId::Verifier);
            };
            b.last().actions.push_back(std::move(highlight));
        }
    }
    b.info("unknown", tr("ui.core.unloaded_positions_remain_unknown"));
    b.info("ignored", tr("ui.core.ignored") + std::to_string(report.ignored));
    size_t groupIndex = 0;
    for (auto const& group : report.groups) {
        b.button(
            "group:" + std::to_string(groupIndex++),
            (group.ignored ? tr("ui.core.ignored_prefix") : std::string()) + group.expected + " → " + group.actual
                + " ×" + std::to_string(group.count),
            [] {}
        );
        b.last().kind      = RowKind::TableRow;
        b.last().textColor = group.ignored ? colors::Gray : colors::Default;
        Row ignore;
        ignore.id     = b.last().id + ":ignore";
        ignore.label  = tr("ui.core.ignore");
        ignore.submit = [p, id, key = group.id](std::string) {
            auto s = app::accessSession();
            if (s) s->verifier().ignoreGroup(id, key);
            refresh(p, PageId::Verifier);
        };
        b.last().actions.push_back(std::move(ignore));
    }
    size_t locationIndex = 0;
    for (auto const& location : report.nearby)
        b.info(
            "near:" + std::to_string(locationIndex++),
            tr(verifier::verificationStatusKey(location.status)) + " (" + std::to_string(location.position.x) + ", "
                + std::to_string(location.position.y) + ", " + std::to_string(location.position.z) + ")"
        );
}
void sessions(std::shared_ptr<Page> const& p) {
    p->title      = tr("ui.core.session_recovery");
    p->layout     = PageLayout::List;
    p->searchable = false;
    auto s        = app::accessSession();
    if (!s) return;
    auto        status = s->sessions().status();
    PageBuilder b{p};
    b.info("identity", status.identity);
    b.info("session_status", status.detail);
    b.info("saved_count", tr("ui.core.saved_placements") + std::to_string(status.placements));
    b.button("restore", tr("ui.core.restore_dimension"), [p] {
         confirm(p, tr("ui.core.replace_current_dimension"), PageId::Session, [p] {
             auto s = app::accessSession();
             if (!s) return;
             p->message = s->sessions().requestRestore() ? tr("ui.core.restore_queued")
                                                         : tr("ui.core.no_pending_record_or_busy");
             refresh(p, PageId::Session);
         });
     }).in(Slot::FooterLeft);
    b.button("keep_current", tr("ui.core.keep_current"), [p] {
         confirm(p, tr("ui.core.replace_saved_record_with_current"), PageId::Session, [p] {
             auto s = app::accessSession();
             if (s) s->sessions().keepCurrent();
             refresh(p, PageId::Session);
         });
     }).in(Slot::FooterLeft);
    b.button("cancel_restore", tr("ui.core.cancel_restore"), [p] {
         auto s = app::accessSession();
         if (s) s->sessions().cancelRestore();
         refresh(p, PageId::Session);
     }).in(Slot::FooterLeft);
    b.button("session_refresh", tr("ui.core.refresh_status"), [p] { refresh(p, PageId::Session); }).in(Slot::TopBar);
}
} // namespace
void populateCorePage(std::shared_ptr<Page> const& p, PageId id) {
    switch (id) {
    case PageId::SaveSelection:
        save(p, std::make_shared<app::SaveOptions>());
        break;
    case PageId::Layers:
        layers(p);
        break;
    case PageId::Verifier:
        verifier(p);
        break;
    case PageId::Session:
        sessions(p);
        break;
    default:
        break;
    }
    if (id == PageId::SaveSelection) return;
    PageBuilder b{p};
    if (id == PageId::Verifier)
        navButton(b, "placement_config", tr("ui.core.placement_configuration"), PageId::Placement, Slot::FooterLeft);
    mainMenuButton(b);
}
} // namespace levischematic::ui
