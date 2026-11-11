#include "OrganizationPages.h"
#include "UiI18n.h"
#include "levischematic/app/AppKernel.h"
#include "levischematic/app/ProjectionRefresh.h"
#include "levischematic/render/ActorProjection.h"
#include "levischematic/util/Utf8Path.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/string/HashedString.h"
#include <charconv>

namespace levischematic::ui {
namespace {
bool integer(std::string const& text, int& value) {
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}
BlockPos selectionOrigin(selection::SelectionState const& state) {
    if (state.customOrigin) return *state.customOrigin;
    std::optional<BlockPos> minimum;
    auto                    include = [&](std::optional<BlockPos> a, std::optional<BlockPos> b) {
        if (!a || !b) return;
        BlockPos at{std::min(a->x, b->x), std::min(a->y, b->y), std::min(a->z, b->z)};
        if (!minimum) minimum = at;
        else minimum = BlockPos{std::min(minimum->x, at.x), std::min(minimum->y, at.y), std::min(minimum->z, at.z)};
    };
    if (state.normal)
        for (auto const& r : state.regions) include(r.corner1, r.corner2);
    else include(state.corner1, state.corner2);
    return minimum.value_or(BlockPos{0, 0, 0});
}
void confirm(std::shared_ptr<Page> const& p, std::string message, uint64_t resource, std::function<void()> action) {
    beginModal(p, std::move(message));
    PageBuilder b{p};
    b.button("save_first", "先保存再继续 / Save and continue", [p, resource, action] {
        auto s = app::accessSession();
        if (!s) return;
        auto result = s->placement().resources().save(resource, {}, true);
        if (!result) {
            p->message = result.error();
            return;
        }
        dismissModal(p);
        action();
    });
    b.button("confirm", "丢弃并继续 / Discard and continue", [p, action = std::move(action)] { dismissModal(p); action(); }).button("cancel", "取消 / Cancel", [p] {
        dismissModal(p);
    });
}
void changed(std::shared_ptr<Page> const& p, app::PlacementMutationResult const& result) {
    p->message = result ? "已更新 / Updated" : result.error().describe();
    if (result)
        if (auto s = app::accessSession()) app::refreshCurrentClientProjectionState(*s);
}
void resources(std::shared_ptr<Page> const& p) {
    auto s = app::accessSession();
    if (!s) return;
    p->title = "结构资源 / Schematic resources";
    PageBuilder b{p};
    auto        records = s->placement().resources().list();
    b.input("save_as", "当前放置资源另存为 / Save selected resource as", "", [p](std::string text) {
        auto s = app::accessSession();
        if (!s) return;
        auto id = s->placement().selectedPlacementId();
        if (!id || text.empty()) {
            p->message = "请先选择放置并输入文件名 / Select a placement and enter a filename";
            return;
        }
        auto path   = s->runtime().resolveSchematicPath(text);
        auto result = s->placement().resources().saveAs(*id, path, false);
        p->message  = result ? "已另存并切换当前放置 / Saved; current placement uses the new resource" : result.error();
        if (result) app::refreshCurrentClientProjectionState(*s);
    });
    for (auto const& r : records) {
        auto prefix = "resource:" + std::to_string(r.id) + ":";
        b.info(
            prefix + "info",
            r.content->defaultName + (r.content->modified ? " *" : "") + "\n" + r.path.string()
                + "\nregions=" + std::to_string(r.content->regions.empty() ? 1 : r.content->regions.size())
                + " blocks=" + std::to_string(r.content->totalNonAirBlocks())
                + (r.content->sourceConflict ? "\n源文件已变更，请另存 / Source changed; Save As required" : "")
        );
        b.button(prefix + "place", "创建放置 / Create placement", [p, id = r.id] {
            auto s      = app::accessSession();
            auto client = ll::service::getClientInstance();
            auto player = client ? client->getLocalPlayer() : nullptr;
            if (!s || !player) return;
            auto result = s->placement().resources().createPlacement(
                id,
                BlockPos{player->getPosition()},
                static_cast<int>(player->getDimensionId())
            );
            p->message = result ? "已创建 / Created" : result.error();
            if (result) app::refreshCurrentClientProjectionState(*s);
        });
        b.button(prefix + "save", "保存投影内容 / Save schematic", [p, id = r.id] {
            auto s = app::accessSession();
            if (!s) return;
            auto result = s->placement().resources().save(id, {}, true);
            p->message  = result ? "已保存 / Saved" : result.error();
        });
        b.button(prefix + "reload", "重新加载 / Reload", [p, id = r.id] {
            confirm(p, "重新加载将丢弃本资源的未保存编辑 / Reload discards unsaved edits", id, [p, id] {
                auto s = app::accessSession();
                if (!s) return;
                auto result = s->placement().resources().reload(id, true);
                p->message  = result ? "已重载 / Reloaded" : result.error();
                if (result) app::refreshCurrentClientProjectionState(*s);
            });
        });
        b.button(prefix + "unload", "卸载资源及引用放置 / Unload with placements", [p, id = r.id] {
            auto s = app::accessSession();
            if (!s) return;
            std::string references;
            for (auto const& ref : s->placement().managedPlacements())
                if (ref.get().asset->resourceId == id)
                    references += "\n" + std::to_string(ref.get().id) + " " + ref.get().name;
            confirm(p, "丢弃未保存编辑并删除以下放置 / Discard edits and remove placements:" + references, id, [p, id] {
                auto s = app::accessSession();
                if (!s) return;
                auto result = s->placement().resources().unload(id, true, true);
                p->message  = result ? "已卸载 / Unloaded" : result.error();
                if (result) app::refreshCurrentClientProjectionState(*s);
            });
        });
    }
    b.button("load_browser", "加载结构 / Load schematics", [] { requestOpen(PageId::Load); });
    b.button("refresh", "刷新 / Refresh", [] { requestOpen(PageId::Resources); });
}
} 
void appendPlacementOrganization(std::shared_ptr<Page> const& p, uint32_t id) {
    auto s = app::accessSession();
    if (!s) return;
    auto item = s->placement().findPlacement(id);
    if (!item) return;
    PageBuilder b{p};
    b.input("name", "放置名称 / Placement name", item->name, [p, id](std::string text) {
        if (auto s = app::accessSession()) changed(p, s->placement().renamePlacement(id, std::move(text)));
    });
    auto lockRow = [&](std::string key, std::string label, int axis, bool value) {
        b.input(key, label, value ? "true" : "false", [p, id, axis](std::string) {
            auto s = app::accessSession();
            if (!s) return;
            auto item = s->placement().findPlacement(id);
            if (!item) return;
            auto axes   = item->axisLocked;
            bool locked = item->locked;
            if (axis < 0) locked = !locked;
            else axes[axis] = !axes[axis];
            changed(p, s->placement().setPlacementLocks(id, locked, axes));
            requestOpen(PageId::Placement);
        });
        p->rows.back().kind = RowKind::Toggle;
    };
    lockRow("locked", "锁定放置 / Lock placement", -1, item->locked);
    for (int axis = 0; axis < 3; ++axis)
        lockRow(
            "lock:" + std::to_string(axis),
            std::string("锁定 ") + "XYZ"[axis] + " / Lock " + "XYZ"[axis],
            axis,
            item->axisLocked[axis]
        );
    b.input("entities", "普通实体 / Actors", item->entitiesEnabled ? "true" : "false", [p, id](std::string) {
        auto s = app::accessSession();
        if (!s) return;
        auto item = s->placement().findPlacement(id);
        if (!item) return;
        changed(p, s->placement().setPlacementEntities(id, !item->entitiesEnabled));
        requestOpen(PageId::Placement);
    });
    p->rows.back().kind = RowKind::Toggle;
    b.input("grid:enabled", "重复网格 / Repeated grid", item->grid.enabled ? "true" : "false", [p, id](std::string) {
        auto s = app::accessSession();
        if (!s) return;
        auto item = s->placement().findPlacement(id);
        if (!item) return;
        auto grid    = item->grid;
        grid.enabled = !grid.enabled;
        changed(p, s->placement().setPlacementGrid(id, grid));
        requestOpen(PageId::Placement);
    });
    p->rows.back().kind = RowKind::Toggle;
    for (int axis = 0; axis < 3; ++axis)
        for (int field = 0; field < 3; ++field) {
            int value = field == 0 ? item->grid.spacing[axis]
                      : field == 1 ? item->grid.negative[axis]
                                   : item->grid.positive[axis];
            b.input(
                "grid:" + std::to_string(axis) + ":" + std::to_string(field),
                std::string("XYZ").substr(axis, 1)
                    + (field == 0   ? " 间距 / Spacing"
                       : field == 1 ? " 负向次数 / Negative copies"
                                    : " 正向次数 / Positive copies"),
                std::to_string(value),
                [p, id, axis, field](std::string text) {
                    int value;
                    if (!integer(text, value)) {
                        p->message = "请输入整数 / Integer required";
                        return;
                    }
                    auto s = app::accessSession();
                    if (!s) return;
                    auto item = s->placement().findPlacement(id);
                    if (!item) return;
                    auto grid                                                                      = item->grid;
                    (field == 0 ? grid.spacing : field == 1 ? grid.negative : grid.positive)[axis] = value;
                    changed(p, s->placement().setPlacementGrid(id, grid));
                }
            );
        }
    for (auto const& r : item->asset->regions) {
        auto settings = item->regions.contains(r.id) ? item->regions.at(r.id) : placement::RegionSettings{};
        auto submit   = [p, id, region = r.id](int field, int value) {
            auto s = app::accessSession();
            if (!s) return;
            auto item = s->placement().findPlacement(id);
            if (!item) return;
            auto current = item->regions.contains(region) ? item->regions.at(region) : placement::RegionSettings{};
            if (field == 0) current.enabled = !current.enabled;
            else if (field == 1) current.rotation = value;
            else if (field == 2) current.mirror = value;
            else if (field == 3) current.offset.x = value;
            else if (field == 4) current.offset.y = value;
            else current.offset.z = value;
            changed(p, s->placement().setPlacementRegion(id, region, current));
        };
        for (int field = 0; field < 6; ++field) {
            int value = field == 0 ? settings.enabled
                      : field == 1 ? settings.rotation
                      : field == 2 ? settings.mirror
                      : field == 3 ? settings.offset.x
                      : field == 4 ? settings.offset.y
                                   : settings.offset.z;
            b.input(
                "region:" + r.id + ":" + std::to_string(field),
                r.name + " / "
                    + std::array{"Enabled", "Rotation 0..3", "Mirror 0..2", "Offset X", "Offset Y", "Offset Z"}[field],
                std::to_string(value),
                [p, submit, field](std::string text) {
                    int value;
                    if (integer(text, value)) submit(field, value);
                    else p->message = "Integer required";
                }
            );
        }
    }
    b.button("resources", "结构资源 / Resources", [] { requestOpen(PageId::Resources); });
    b.button("edit", "编辑投影 / Edit schematic", [] { requestOpen(PageId::Edit); });
}
void appendSelectionOrganization(std::shared_ptr<Page> const& p) {
    auto s = app::accessSession();
    if (!s) return;
    auto const& state = s->selection().state();
    PageBuilder b{p};
    b.info("selection:grow_status", s->selection().growStatus());
    b.button("selection:grow", "自动 Grow / Grow to surrounding blocks", [p] {
        auto s      = app::accessSession();
        auto client = ll::service::getClientInstance();
        auto player = client ? client->getLocalPlayer() : nullptr;
        if (s && player)
            p->message = s->selection().startGrow(static_cast<int>(player->getDimensionId())) ? "Grow started"
                                                                                              : "Incomplete selection";
    });
    b.button("selection:grow_cancel", "取消 Grow / Cancel Grow", [] {
        if (auto s = app::accessSession()) s->selection().cancelGrow();
    });
    b.input(
        "selection:normal",
        "多区域 Normal 模式 / Normal selection",
        state.normal ? "true" : "false",
        [](std::string) {
            if (auto s = app::accessSession()) s->selection().setNormal(!s->selection().state().normal);
            requestOpen(PageId::Selection);
        }
    );
    p->rows.back().kind = RowKind::Toggle;
    b.input("selection:new", "新增区域 / Add region", "", [p](std::string name) {
        auto s = app::accessSession();
        if (!s) return;
        p->message =
            s->selection().addRegion(std::move(name)) ? "已添加 / Added" : "名称为空或重复 / Empty or duplicate name";
        requestOpen(PageId::Selection);
    });
    for (auto const& r : state.regions) {
        auto key = "selection:region:" + std::to_string(r.id);
        b.button(key, "选择 / Select " + r.name, [id = r.id] {
            if (auto s = app::accessSession()) (void)s->selection().selectRegion(id);
            requestOpen(PageId::Selection);
        });
        b.input(key + ":name", "区域名称 / Region name", r.name, [id = r.id](std::string name) {
            if (auto s = app::accessSession()) (void)s->selection().renameRegion(id, std::move(name));
        });
        b.button(key + ":remove", "删除区域 / Remove region", [id = r.id] {
            if (auto s = app::accessSession()) (void)s->selection().removeRegion(id);
            requestOpen(PageId::Selection);
        });
    }
    auto origin = selectionOrigin(state);
    for (int axis = 0; axis < 3; ++axis) {
        int value = axis == 0 ? origin.x : axis == 1 ? origin.y : origin.z;
        b.input(
            "selection:origin:" + std::to_string(axis),
            std::string("原点 ") + "XYZ"[axis] + " / Origin " + "XYZ"[axis],
            std::to_string(value),
            [axis](std::string text) {
                int n;
                if (!integer(text, n)) return;
                auto s = app::accessSession();
                if (!s) return;
                auto value = selectionOrigin(s->selection().state());
                if (axis == 0) value.x = n;
                else if (axis == 1) value.y = n;
                else value.z = n;
                s->selection().setOrigin(value);
            }
        );
        b.input(
            "selection:resize:" + std::to_string(axis),
            std::string("扩展/收缩 ") + "XYZ"[axis] + " / Expand/shrink " + "XYZ"[axis],
            "0",
            [p, axis](std::string text) {
                int n;
                if (!integer(text, n)) return;
                if (auto s = app::accessSession())
                    p->message = s->selection().resize({axis == 0 ? n : 0, axis == 1 ? n : 0, axis == 2 ? n : 0})
                                   ? "已调整 / Resized"
                                   : "选区无效或溢出 / Invalid selection or overflow";
            }
        );
    }
    b.button("selection:auto_origin", "自动原点 / Automatic origin", [] {
        if (auto s = app::accessSession()) s->selection().setOrigin({});
    });
    b.button("selection:from_placement", "从当前放置创建选区 / Selection from placement", [p] {
        auto s = app::accessSession();
        if (!s) return;
        auto id    = s->placement().selectedPlacementId();
        p->message = id && s->selection().fromPlacements(s->placement().orderedPlacements(), *id)
                       ? "已创建 / Created"
                       : "没有放置 / No placement";
    });
}
void populateOrganizationPage(std::shared_ptr<Page> const& p, PageId id) {
    if (id == PageId::Resources) resources(p);
    else {
        auto s = app::accessSession();
        if (!s) return;
        p->title = "编辑投影 / Edit schematic";
        PageBuilder b{p};
        b.info(
            "help",
            uiChinese() ? "持木棍：中键采样投影状态，左键删除，右键写入采样状态。修改共享资源；范围受当前渲染层限制。" : "Stick: middle samples, left removes, right writes. All placements share edits; limited by render layers."
        );
        b.button("mode", "启用编辑模式 / Enable edit mode", [] {
            if (auto s = app::accessSession()) s->tools().setMode(editor::ToolMode::Edit);
        });
        b.input("replacement", "写入方块 / Replacement block ID", "", [p](std::string name) {
            auto block = Block::tryGetFromRegistry(HashedString{name});
            if (!block) {
                p->message = "方块不存在 / Unknown block";
                return;
            }
            if (auto s = app::accessSession()) s->tools().sampleBlock(&*block);
            p->message = "已选择写入方块 / Replacement selected";
        });
        b.input(
            "match",
            "编辑范围 / Edit match",
            std::to_string(static_cast<int>(s->tools().state().editMatch)),
            [](std::string text) {
                int value;
                if (!integer(text, value) || value < 0 || value > 3) return;
                if (auto s = app::accessSession())
                    s->tools().configureEdit(static_cast<placement::EditMatch>(value), s->tools().state().editAdjacent);
            }
        );
        p->rows.back().kind = RowKind::Choice;
        p->rows.back()
            .choices = {"单块 / Single", "同状态 / Same state", "同类型 / Same type", "连续行 / Contiguous line"};
        b.input(
            "adjacent",
            "右键邻接添加 / Add adjacent",
            s->tools().state().editAdjacent ? "true" : "false",
            [](std::string) {
                if (auto s = app::accessSession())
                    s->tools().configureEdit(s->tools().state().editMatch, !s->tools().state().editAdjacent);
            }
        );
        p->rows.back().kind = RowKind::Toggle;
        auto progress       = s->placement().resources().progress();
        b.info(
            "progress",
            progress.detail + " " + std::to_string(progress.scanned) + " / " + std::to_string(progress.total)
                + " changed=" + std::to_string(progress.changed)
        );
        b.button("cancel", "取消编辑任务 / Cancel edit", [] {
            if (auto s = app::accessSession()) s->placement().resources().cancel();
        });
        auto stats = s->actors().stats();
        b.info(
            "actors",
            "Actors: " + std::to_string(stats.loaded) + " submitted=" + std::to_string(stats.submitted)
                + " highlighted=" + std::to_string(stats.highlighted) + " " + stats.detail
        );
        b.button("refresh", "刷新 / Refresh", [p, id] {
            p->rows.clear();
            populateOrganizationPage(p, id);
        });
        b.button("resources", "保存/重载资源 / Save/reload resources", [] { requestOpen(PageId::Resources); });
    }
    PageBuilder{p}.button("organization:main", "主菜单 / Main menu", [] { requestOpen(PageId::MainMenu); });
}
} 
