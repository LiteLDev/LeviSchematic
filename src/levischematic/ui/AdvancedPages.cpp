#include "AdvancedPages.h"
#include "UiI18n.h"
#include "levischematic/app/AppKernel.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include <charconv>
namespace levischematic::ui {
namespace {
char const* operationName(worldedit::Operation op) {
    constexpr char const* names[]{
        "Paste / 粘贴",
        "Grid Paste / 网格粘贴",
        "Fill / 填充",
        "Replace / 替换",
        "Delete / 删除",
        "Clone / 复制",
        "Move / 移动"
    };
    return names[static_cast<int>(op)];
}
void result(std::shared_ptr<Page> const& p, app::StatusResult<std::string> const& r) {
    p->message = r ? "Done / 已提交" : r.error();
}
void world(std::shared_ptr<Page> const& p) {
    p->title = "创造世界操作 / World operations";
    PageBuilder b{p};
    auto        s = app::accessSession();
    if (!s) return;
    auto options = s->worldEdit().options();
    b.info(
        "world_warning",
        uiChinese() ? "真实世界修改；取消不撤销已执行部分。先预检，再执行。" : "Actual world changes; cancel keeps completed writes. Prepare before starting."
    );
    b.info(
        "interaction_help",
        uiChinese() ? "持工具中键采样目标块；替换来源采样动作可改绑。执行时靠近并瞄准目标/支撑面。" : "Middle tool click samples primary; bind secondary sample in Key bindings. Aim at the target or support face."
    );
    for (auto [name, secondary] : {
             std::pair{"primary",   false},
             {"secondary", true }
    })
        b.button(
            name,
            secondary ? "从准星采样来源 / Sample source at aim" : "从准星采样目标 / Sample target at aim",
            [secondary] {
                auto s = app::accessSession();
                auto c = ll::service::getClientInstance();
                if (s && c) s->worldEdit().sample(*c, secondary);
            }
        );
    b.button("operation", std::string("Operation: ") + operationName(options.operation), [p] {
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
    b.input("policy", "替换策略 / Replacement", std::to_string(static_cast<int>(options.policy)), [](std::string text) {
        int n;
        auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), n);
        if (ec != std::errc{} || end != text.data() + text.size() || n < 0 || n > 2) return;
        auto s = app::accessSession();
        if (s) s->worldEdit().options().policy = static_cast<worldedit::ReplacePolicy>(n);
    });
    p->rows.back().kind = RowKind::Choice;
    p->rows.back()
        .choices = {"仅目标空气 / Target air", "结构非空气 / Source non-air", "全部有效位置 / All valid cells"};
    for (int i = 0; i < 3; ++i)
        b.input(
            "offset" + std::to_string(i),
            localizedText("复制/移动偏移 / Clone/Move offset") + " " + "XYZ"[i],
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
    b.button(
        "match_type",
        options.matchTypeOnly ? "替换匹配：方块类型 / Match type" : "替换匹配：完整状态 / Match state",
        [] {
            auto s = app::accessSession();
            if (s) s->worldEdit().options().matchTypeOnly = !s->worldEdit().options().matchTypeOnly;
            requestOpen(PageId::WorldOperations);
        }
    );
    b.button(
        "layer",
        options.layerOnly ? "范围：当前渲染层 / Current layers" : "范围：全部有效区域 / Full valid area",
        [] {
            auto s = app::accessSession();
            if (s) s->worldEdit().options().layerOnly = !s->worldEdit().options().layerOnly;
            requestOpen(PageId::WorldOperations);
        }
    );
    b.button("prepare", "预检 / Prepare", [p] {
        auto s = app::accessSession();
        auto c = ll::service::getClientInstance();
        if (!s || !c) return;
        auto r     = s->worldEdit().prepare(*c, s->sessionGeneration());
        p->message = r ? "Preflight queued." : r.error();
    });
    b.button("start", "执行已预检任务 / Start prepared task", [] {
        auto s = app::accessSession();
        if (s) s->worldEdit().start();
    });
    b.button("pause", "暂停 / Pause", [] {
        auto s = app::accessSession();
        if (s) s->worldEdit().pause();
    });
    b.button("resume", "继续 / Resume", [] {
        auto s = app::accessSession();
        if (s) s->worldEdit().resume();
    });
    b.button("cancel", "取消 / Cancel", [] {
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
                row.label = std::string(worldedit::phaseName(q.phase)) + " #" + std::to_string(q.id)
                          + " prepared=" + std::to_string(q.prepared) + " observed=" + std::to_string(q.observed) + "/"
                          + std::to_string(q.total) + " verified=" + std::to_string(q.verified) + " skipped="
                          + std::to_string(q.skipped) + " rejected=" + std::to_string(q.rejected) + "\n" + q.detail;
    };
    p->refresh();
    moveActionsToFooter(*p, {"prepare", "start", "cancel"});
}
void projects(std::shared_ptr<Page> const& p) {
    p->title = "建筑项目 / Projects";
    PageBuilder b{p};
    auto        s = app::accessSession();
    if (!s) return;
    auto view = s->projects().view();
    auto name = std::make_shared<std::string>(view.name), version = std::make_shared<std::string>("Version"),
         description = std::make_shared<std::string>();
    b.input("project_name", "项目名称 / Project name", *name, [name](std::string value) { *name = std::move(value); });
    b.button("create", "以玩家位置创建 / Create at player", [p, name] {
        auto s = app::accessSession();
        auto c = ll::service::getClientInstance();
        if (s && c && c->getLocalPlayer())
            result(p, s->projects().create(*name, BlockPos(c->getLocalPlayer()->getPosition())));
    });
    b.button("open", "打开 / Open", [p, name] {
        auto s = app::accessSession();
        if (s) {
            result(p, s->projects().open(*name));
            if (p->message == "Done / 已提交") requestOpen(PageId::Projects);
        }
    });
    b.button("rename", "重命名 / Rename", [p, name] {
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
        b.button("project_origin", "原点设为玩家位置 / Origin at player", [p] {
            auto s = app::accessSession();
            auto c = ll::service::getClientInstance();
            if (s && c && c->getLocalPlayer())
                result(p, s->projects().setOrigin(BlockPos(c->getLocalPlayer()->getPosition())));
        });
        b.input("version_name", "版本名称 / Version name", *version, [version](std::string v) {
            *version = std::move(v);
        });
        b.input("description", "版本说明 / Description", *description, [description](std::string v) {
            *description = std::move(v);
        });
        b.button("save_version", "保存当前世界选区为版本 / Snapshot world selection", [p, version, description] {
            auto s = app::accessSession();
            if (s) result(p, s->projects().saveVersion(*s, *version, *description));
        });
        b.input("work", "工作进度 / Work note", view.work, [p](std::string text) {
            auto s = app::accessSession();
            if (s) result(p, s->projects().setWork(std::move(text)));
        });
        for (auto const& v : view.versions)
            b.button(
                "version:" + std::to_string(v.id),
                std::to_string(v.id) + " " + v.name + " " + v.description,
                [p, id = v.id] {
                    auto s = app::accessSession();
                    if (s) result(p, s->projects().checkout(*s, id));
                }
            );
        b.button("paste_version", "预检当前版本粘贴 / Prepare version paste", [p] {
            auto s = app::accessSession();
            auto c = ll::service::getClientInstance();
            if (s && c) result(p, s->projects().paste(*s, *c));
        });
        b.button("restore_selection", "恢复版本选区 / Restore version selection", [p] {
            auto s = app::accessSession();
            if (s) result(p, s->projects().restoreSelection(*s));
        });
        for (auto [name, direction] : {
                 std::pair{"previous", -1},
                 {"next",     1 }
        })
            b.button(name, direction < 0 ? "上一版本 / Previous version" : "下一版本 / Next version", [p, direction] {
                auto s = app::accessSession();
                if (!s) return;
                auto view = s->projects().view();
                result(p, s->projects().checkout(*s, view.current + direction));
            });
        b.button("project_cancel", "取消项目任务 / Cancel project task", [] {
            auto s = app::accessSession();
            if (s) s->projects().cancel(*s);
        });
    }
    b.button("refresh", "刷新列表 / Refresh", [] { requestOpen(PageId::Projects); });
    b.info("project_status", view.detail);
    p->refresh = [p] {
        auto s = app::accessSession();
        if (s)
            for (auto& row : p->rows)
                if (row.id == "project_status") row.label = s->projects().view().detail;
    };
}
void signs(std::shared_ptr<Page> const& p) {
    p->title = "告示牌文本 / Sign text";
    PageBuilder b{p};
    for (auto [id, back] : {
             std::pair{"front", false},
             {"back",  true }
    })
        b.button(id, back ? "复制背面 / Copy back" : "复制正面 / Copy front", [p, back] {
            auto s = app::accessSession();
            auto c = ll::service::getClientInstance();
            if (s && c) p->message = s->signs().copy(*c, s->placement(), back);
        });
    b.button("paste", "粘贴到下个正常打开的告示牌 / Paste into next opened sign", [p] {
        auto s = app::accessSession();
        if (s) p->message = s->signs().arm();
    });
    b.button("cancel_paste", "取消待粘贴 / Cancel pending paste", [] {
        auto s = app::accessSession();
        if (s) s->signs().cancel();
    });
    auto s = app::accessSession();
    if (s) b.info("clipboard", s->signs().text());
}
} 
void populateAdvancedPage(std::shared_ptr<Page> const& p, PageId id) {
    if (id == PageId::WorldOperations) world(p);
    else if (id == PageId::Projects) projects(p);
    else signs(p);
}
} 
