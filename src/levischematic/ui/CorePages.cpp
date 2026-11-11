#include "CorePages.h"
#include "levischematic/app/AppKernel.h"
#include "levischematic/app/ProjectionRefresh.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include <charconv>
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
        .button("confirm", "确认 / Confirm", [page, action = std::move(action)] { dismissModal(page); action(); })
        .button("cancel", "返回 / Back", [page] { dismissModal(page); });
}
char const* modeLabel(editor::LayerMode mode) {
    switch (mode) {
    case editor::LayerMode::All:
        return "全部 / All";
    case editor::LayerMode::Single:
        return "单层 / Single";
    case editor::LayerMode::Below:
        return "以下 / Below";
    case editor::LayerMode::Above:
        return "以上 / Above";
    case editor::LayerMode::Range:
        return "范围 / Range";
    }
    return "All";
}
void layers(std::shared_ptr<Page> const& p) {
    p->title = "分层渲染 / Render layers";
    auto s   = app::accessSession();
    if (!s) return;
    auto        range = s->view().currentLayerRange();
    PageBuilder b{p};
    b.button("mode", modeLabel(range.mode), [p] {
        auto s = app::accessSession();
        if (!s) return;
        s->view().cycleMode(1);
        app::refreshCurrentClientProjectionState(*s);
        refresh(p, PageId::Layers);
    });
    b.input("axis", "坐标轴 / Axis", std::to_string(static_cast<int>(range.axis)), [p](std::string text) {
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
             std::tuple{"boundary", "单层/边界 / Layer",  range.boundary()},
             {"minimum",  "范围下界 / Minimum", range.minY      },
             {"maximum",  "范围上界 / Maximum", range.maxY      }
    }) {
        b.input(id, label, std::to_string(value), [p, id = std::string(id)](std::string text) {
            int n;
            if (!integer(text, n)) {
                p->message = "请输入整数 / Integer required";
                return;
            }
            auto s = app::accessSession();
            if (!s) return;
            auto range = s->view().currentLayerRange();
            if (id == "boundary") range.boundary() = n;
            else if (id == "minimum") range.minY = n;
            else range.maxY = n;
            auto result = s->view().configure(range);
            p->message  = result ? "已更新 / Updated" : result.error().describe();
            app::refreshCurrentClientProjectionState(*s);
            refresh(p, PageId::Layers);
        });
    }
    b.input(
        "follow",
        "跟随玩家（单层/上下界） / Follow player",
        range.followPlayer ? "true" : "false",
        [p](std::string) {
            auto s = app::accessSession();
            if (!s) return;
            auto range         = s->view().currentLayerRange();
            range.followPlayer = !range.followPlayer;
            (void)s->view().configure(range);
            refresh(p, PageId::Layers);
        }
    );
    p->rows.back().kind = RowKind::Toggle;
    for (bool minimum : {true, false}) {
        b.input(
            minimum ? "move_min" : "move_max",
            minimum ? "翻层移动下界 / Move minimum" : "翻层移动上界 / Move maximum",
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
    b.button("here", "设为玩家所在层 / Set here", [p] {
        auto s      = app::accessSession();
        auto client = ll::service::getClientInstance();
        if (s && client && client->getLocalPlayer()) {
            s->view().setAt(BlockPos(client->getLocalPlayer()->getPosition()));
            app::refreshCurrentClientProjectionState(*s);
            refresh(p, PageId::Layers);
        }
    });
    b.info("help", "PageUp/PageDown：按模式切层；M+PageUp/PageDown：切换模式。全部模式不切层。");
}
void save(std::shared_ptr<Page> const& p, std::shared_ptr<app::SaveOptions> options) {
    p->title = "保存世界选区 / Save world selection";
    auto s   = app::accessSession();
    if (!s) return;
    auto        progress = s->saves().progress();
    PageBuilder b{p};
    b.input("filename", "文件名 / Filename", p->fileName, [p](std::string text) {
        p->fileName = std::move(text);
        for (auto& row : p->rows)
            if (row.id == "filename") row.value = p->fileName;
    });
    b.input(
        "entities",
        "包含实体 / Include entities",
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
            p->message = result ? "保存任务已提交 / Save queued" : result.error();
        } catch (std::exception const& e) {
            p->message = e.what();
        }
        p->rows.clear();
        save(p, options);
    };
    b.button("save", "保存新文件 / Save new file", [submit] { submit(false); });
    b.button("overwrite", "覆盖已有文件… / Overwrite…", [p, submit] {
        confirm(p, "覆盖 " + p->fileName + "？ / Replace existing file?", PageId::SaveSelection, [submit] {
            submit(true);
        });
    });
    b.button("save_status", "刷新进度 / Refresh progress", [p, options] {
        p->rows.clear();
        save(p, options);
    });
    b.button("save_cancel", "取消任务 / Cancel task", [p, options] {
        auto s = app::accessSession();
        if (s) s->saves().cancel();
        p->rows.clear();
        save(p, options);
    });
    b.info(
        "progress",
        progress.phase + " " + std::to_string(progress.completed) + "/" + std::to_string(progress.total)
    );
    b.info("detail", progress.detail);
    b.info("time_span", "跨 tick 渐进采集；世界变化会反映为不同时刻的数据 / Capture spans multiple ticks.");
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
    p->title    = "方块校验 / Schematic verifier";
    auto s      = app::accessSession();
    auto client = ll::service::getClientInstance();
    if (!s || !client || !client->getLocalPlayer()) return;
    auto*       player    = client->getLocalPlayer();
    int         dimension = static_cast<int>(player->getDimensionId());
    PageBuilder b{p};
    auto        selected = s->placement().requireSelectedPlacement();
    if (!selected || selected.value()->dimensionId != dimension) {
        b.info("no_selection", "请选择当前维度的放置 / Select a placement in this dimension");
    }
    for (auto const& ref : s->placement().orderedPlacements()) {
        auto const& value = ref.get();
        if (value.dimensionId != dimension) continue;
        b.button("placement:" + std::to_string(value.id), value.name, [p, id = value.id] {
            auto s = app::accessSession();
            if (!s) return;
            (void)s->placement().selectPlacement(id);
            refresh(p, PageId::Verifier);
        });
    }
    if (!selected || selected.value()->dimensionId != dimension) return;
    auto id = selected.value()->id;
    b.info("selected", "当前放置 / Placement: " + selected.value()->name);
    for (auto const& [key, label, layersOnly] : {
             std::tuple{"start_all",    "开始：完整范围 / Start full",   false},
             {"start_layers", "开始：渲染范围 / Start layers", true }
    })
        b.button(key, label, [p, id, layersOnly] {
            auto s = app::accessSession();
            if (!s) return;
            p->message =
                s->verifier().startTask(id, layersOnly) ? "校验已开始 / Started" : "放置未启用 / Placement disabled";
            refresh(p, PageId::Verifier);
        });
    b.button("stop", "停止 / Stop", [p, id] {
        auto s = app::accessSession();
        if (s) s->verifier().stopTask(id);
        refresh(p, PageId::Verifier);
    });
    b.button("reset", "重置 / Reset", [p, id] {
        auto s = app::accessSession();
        if (s) s->verifier().resetTask(id);
        refresh(p, PageId::Verifier);
    });
    b.button("refresh_report", "刷新报告 / Refresh report", [p] { refresh(p, PageId::Verifier); });
    b.button("clear_ignored", "恢复忽略项 / Clear ignored groups", [p, id] {
        auto s = app::accessSession();
        if (s) s->verifier().clearIgnored(id);
        refresh(p, PageId::Verifier);
    });
    auto report = s->verifier().report(id, BlockPos(player->getPosition()));
    b.info(
        "task",
        std::string(report.running ? "运行中 / Running " : "已停止 / Stopped ")
            + (report.building ? "建立范围 / Building " : "") + (report.stale ? "结果已过期 / Stale " : "")
            + std::to_string(report.scanned) + "/" + std::to_string(report.total)
    );
    for (size_t i = 0; i < report.counts.size(); ++i) {
        auto status = static_cast<verifier::VerificationStatus>(i);
        b.info(
            "count:" + std::to_string(i),
            std::string(verifier::verificationStatusLabel(status)) + ": " + std::to_string(report.counts[i])
        );
        if (i >= 2)
            b.button(
                "highlight:" + std::to_string(i),
                "切换高亮 / Highlight: " + std::string(verifier::verificationStatusLabel(status)),
                [p, id, status] {
                    auto s = app::accessSession();
                    if (s) s->verifier().toggleHighlight(id, status);
                    refresh(p, PageId::Verifier);
                }
            );
    }
    b.info("unknown", "未知位置包含未加载区块，不计为匹配 / Unloaded positions remain Unknown.");
    b.info("ignored", "已忽略 / Ignored: " + std::to_string(report.ignored));
    size_t groupIndex = 0;
    for (auto const& group : report.groups) {
        b.button(
            "group:" + std::to_string(groupIndex++),
            (group.ignored ? "[忽略] " : "") + group.expected + " → " + group.actual + " ×"
                + std::to_string(group.count),
            [p, id, key = group.id] {
                auto s = app::accessSession();
                if (s) s->verifier().ignoreGroup(id, key);
                refresh(p, PageId::Verifier);
            }
        );
    }
    size_t locationIndex = 0;
    for (auto const& location : report.nearby)
        b.info(
            "near:" + std::to_string(locationIndex++),
            std::string(verifier::verificationStatusLabel(location.status)) + " (" + std::to_string(location.position.x)
                + ", " + std::to_string(location.position.y) + ", " + std::to_string(location.position.z) + ")"
        );
}
void sessions(std::shared_ptr<Page> const& p) {
    p->title = "会话恢复 / Session recovery";
    auto s   = app::accessSession();
    if (!s) return;
    auto        status = s->sessions().status();
    PageBuilder b{p};
    b.info("identity", status.identity);
    b.info("session_status", status.detail);
    b.info("saved_count", "记录中的放置数 / Saved placements: " + std::to_string(status.placements));
    b.button("restore", "恢复当前维度… / Restore dimension…", [p] {
        confirm(
            p,
            "替换当前维度的放置、选区和分层设置？其他维度保留。 / Replace current dimension?",
            PageId::Session,
            [p] {
                auto s = app::accessSession();
                if (!s) return;
                p->message = s->sessions().requestRestore() ? "恢复已提交 / Restore queued"
                                                            : "没有待恢复记录或任务正忙 / No pending record or busy";
                refresh(p, PageId::Session);
            }
        );
    });
    b.button("keep_current", "使用当前状态… / Keep current…", [p] {
        confirm(
            p,
            "放弃当前维度的待恢复记录，保存当前状态？ / Replace saved record with current state?",
            PageId::Session,
            [p] {
                auto s = app::accessSession();
                if (s) s->sessions().keepCurrent();
                refresh(p, PageId::Session);
            }
        );
    });
    b.button("cancel_restore", "取消恢复 / Cancel restore", [p] {
        auto s = app::accessSession();
        if (s) s->sessions().cancelRestore();
        refresh(p, PageId::Session);
    });
    b.button("session_refresh", "刷新状态 / Refresh status", [p] { refresh(p, PageId::Session); });
}
} 
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
    PageBuilder{p}.button("core_main_menu", "主菜单 / Main menu", [] { requestOpen(PageId::MainMenu); });
}
} 
