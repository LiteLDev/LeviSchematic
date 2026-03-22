#include "Command.h"

#include "levishematic/core/DataManager.h"
#include "levishematic/render/ProjectionRenderer.h"
#include "levishematic/schematic/placement/PlacementManager.h"
#include "levishematic/schematic/placement/SchematicPlacement.h"
#include "levishematic/selection/SelectionManager.h"
#include "levishematic/LeviShematic.h"

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/OverloadData.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/server/commands/CommandBlockName.h"
#include "mc/server/commands/CommandBlockNameResult.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPositionFloat.h"
#include "mc/world/level/dimension/Dimension.h"

#include <string>
#include <filesystem>

namespace levishematic::command {

// ================================================================
// Logger
// ================================================================
static auto& getLogger() {
    return levishematic::LeviShematic::getInstance().getSelf().getLogger();
}

// ================================================================
// 获取当前维度的 RenderChunkCoordinator
// ================================================================
static std::shared_ptr<RenderChunkCoordinator>
getCoordinator(const CommandOrigin& origin) {
    auto client = ll::service::getClientInstance();
    if (!client || !client->getLevelRenderer()) return nullptr;
    auto dimId = origin.getDimension()->getDimensionId();
    return client->getLevelRenderer()->mRenderChunkCoordinators->at(dimId);
}

// ================================================================
// 命令参数结构体
// ================================================================

// /rendert（测试命令）
struct RenderTestParam {
    CommandBlockName     blockName;
    CommandPositionFloat pos;
    int                  title_date;
    enum class mode { add, single } mode_;
};

// /schem load <文件名> [x y z]
struct SchemLoadParam {
    std::string          filename;
    CommandPositionFloat pos;
};

// /schem load <文件名>（无坐标 = 玩家位置）
struct SchemLoadSimpleParam {
    std::string filename;
};

// /schem move <dx dy dz>
struct SchemMoveParam {
    int dx;
    int dy;
    int dz;
};

// /schem rotate <direction>
struct SchemRotateParam {
    enum class direction { cw90, ccw90, r180 } dir;
};

// /schem mirror <axis>
struct SchemMirrorParam {
    enum class axis { x, z, none } axis_;
};

// /schem remove <id>
struct SchemRemoveParam {
    int id;
};

// /schem select <id>
struct SchemSelectParam {
    int id;
};

// /schem origin <x y z>
struct SchemOriginParam {
    CommandPositionFloat pos;
};

// 临时测试存储
static std::vector<render::ProjEntry> sTestEntries;

// ================================================================
// 注册所有命令
// ================================================================
void registerCommands(bool isClient) {
    auto& dm  = core::DataManager::getInstance();
    auto& pm  = dm.getPlacementManager();

    // ================================================================
    // /rendert — 测试渲染命令（保持与 test.cpp 兼容）
    // ================================================================
    auto& renderCmd =
        ll::command::CommandRegistrar::getInstance(isClient)
            .getOrCreateCommand("rendert", "Projection render test");

    // /rendert <pos> <block> <data> <add|single>
    renderCmd.overload<RenderTestParam>()
        .required("pos")
        .required("blockName")
        .required("title_date")
        .required("mode_")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&       output,
                     RenderTestParam const& param,
                     Command const&       cmd) {
             auto& proj = dm.getProjectionState();
             auto  previousSnapshot = proj.getSnapshot();
             auto  coord = getCoordinator(origin);

             if (param.mode_ == RenderTestParam::mode::single) {
                proj.replaceEntries({render::ProjEntry{
                    BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos)),
                    param.blockName.resolveBlock(param.title_date).mBlock,
                    mce::Color(0.75f, 0.85f, 1.0f, 0.85f)
                }});
                sTestEntries.clear();
            } else {
                sTestEntries.push_back(
                    {BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos)),
                     param.blockName.resolveBlock(param.title_date).mBlock,
                     mce::Color(0.75f, 0.85f, 1.0f, 0.85f)}
                );
                proj.replaceEntries(sTestEntries);
            }

            render::triggerRebuildForProjection(coord, std::move(previousSnapshot));
            output.success("Projection updated");
        });

    // /rendert（无参数 = 清空）
    renderCmd.overload()
        .execute([&](CommandOrigin const& origin, CommandOutput& output) {
            auto previousSnapshot = dm.getProjectionState().getSnapshot();
            dm.getProjectionState().clear();
            sTestEntries.clear();
            render::triggerRebuildForProjection(getCoordinator(origin), std::move(previousSnapshot));
            output.success("Projection cleared");
        });

    // ================================================================
    // /schem — 正式命令框架
    // ================================================================
    auto& schemCmd =
        ll::command::CommandRegistrar::getInstance(isClient)
            .getOrCreateCommand("schem", "LeviSchematic commands");

    // ================================================================
    // /schem load <filename> <pos>
    // ================================================================
    schemCmd.overload<SchemLoadParam>()
        .text("load")
        .required("filename")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&       output,
                     SchemLoadParam const& param,
                     Command const&       cmd) {
            auto coord = getCoordinator(origin);
            BlockPos pos(origin.getExecutePosition(cmd.mVersion, param.pos));

            // 解析文件路径
            auto path = pm.resolveSchematicPath(param.filename);

            auto id = pm.loadAndPlace(path, pos);
            if (id == 0) {
                output.error("Failed to load schematic: {}", param.filename);
                return;
            }

            auto* p = pm.getPlacement(id);
            pm.rebuildAndRefresh(coord);

            output.success("Loaded '{}' at ({}) [ID: {}]",
                p ? p->getName() : param.filename,
                pos.toString(),
                id);
        });

    // ================================================================
    // /schem load <filename>（无坐标 = 玩家位置）
    // ================================================================
    schemCmd.overload<SchemLoadSimpleParam>()
        .text("load")
        .required("filename")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&       output,
                     SchemLoadSimpleParam const& param) {
            auto coord = getCoordinator(origin);
            BlockPos pos(origin.getWorldPosition());

            auto path = pm.resolveSchematicPath(param.filename);

            auto id = pm.loadAndPlace(path, pos);
            if (id == 0) {
                output.error("Failed to load schematic: {}", param.filename);
                return;
            }

            auto* p = pm.getPlacement(id);
            pm.rebuildAndRefresh(coord);

            output.success("Loaded '{}' at player position ({}) [ID: {}]",
                p ? p->getName() : param.filename,
                pos.toString(),
                id);
        });

    // ================================================================
    // /schem move <dx> <dy> <dz>
    // ================================================================
    schemCmd.overload<SchemMoveParam>()
        .text("move")
        .required("dx")
        .required("dy")
        .required("dz")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&       output,
                     SchemMoveParam const& param) {
            auto* sel = pm.getSelected();
            if (!sel) {
                output.error("No placement selected. Use /schem load or /schem select first.");
                return;
            }

            sel->move(param.dx, param.dy, param.dz);
            pm.rebuildAndRefresh(getCoordinator(origin));

            auto pos = sel->getOrigin();

            output.success("Moved '{}' by ({},{},{}) → ({})",
                sel->getName(),
                param.dx,
                param.dy,
                param.dz,
                pos.toString());
        });

    // ================================================================
    // /schem origin <x y z>
    // ================================================================
    schemCmd.overload<SchemOriginParam>()
        .text("origin")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&       output,
                     SchemOriginParam const& param,
                     Command const&       cmd) {
            auto* sel = pm.getSelected();
            if (!sel) {
                output.error("No placement selected.");
                return;
            }

            BlockPos pos(origin.getExecutePosition(cmd.mVersion, param.pos));
            sel->setOrigin(pos);
            pm.rebuildAndRefresh(getCoordinator(origin));

            output.success("Set origin of '{}' to ({})", sel->getName(), pos.toString());
        });

    // ================================================================
    // /schem rotate <cw90|ccw90|r180>
    // ================================================================
    schemCmd.overload<SchemRotateParam>()
        .text("rotate")
        .required("dir")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&       output,
                     SchemRotateParam const& param) {
            auto* sel = pm.getSelected();
            if (!sel) {
                output.error("No placement selected.");
                return;
            }

            switch (param.dir) {
            case SchemRotateParam::direction::cw90:
                sel->rotateCW90();
                break;
            case SchemRotateParam::direction::ccw90:
                sel->rotateCCW90();
                break;
            case SchemRotateParam::direction::r180:
                sel->rotate180();
                break;
            }

            pm.rebuildAndRefresh(getCoordinator(origin));
            output.success("Rotated '{}' → {}", sel->getName(), sel->describeTransform());
        });

    // ================================================================
    // /schem mirror <x|z|none>
    // ================================================================
    schemCmd.overload<SchemMirrorParam>()
        .text("mirror")
        .required("axis_")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&       output,
                     SchemMirrorParam const& param) {
            auto* sel = pm.getSelected();
            if (!sel) {
                output.error("No placement selected.");
                return;
            }

            switch (param.axis_) {
            case SchemMirrorParam::axis::x:
                sel->setMirror(transform::Mirror::MIRROR_X);
                break;
            case SchemMirrorParam::axis::z:
                sel->setMirror(transform::Mirror::MIRROR_Z);
                break;
            case SchemMirrorParam::axis::none:
                sel->setMirror(transform::Mirror::NONE);
                break;
            }

            pm.rebuildAndRefresh(getCoordinator(origin));
            output.success("Mirror '{}' → {}", sel->getName(), sel->describeTransform());
        });

    // ================================================================
    // /schem list
    // ================================================================
    schemCmd.overload()
        .text("list")
        .execute([&](CommandOrigin const&, CommandOutput& output) {
            auto& placements = pm.getAllPlacements();
            if (placements.empty()) {
                output.success("No placements loaded.");
                return;
            }

            std::string msg = "§eLoaded placements (" + std::to_string(placements.size()) + "):§r\n";
            for (const auto& p : placements) {
                bool selected = (p.getId() == pm.getSelectedId());
                auto pos = p.getOrigin();

                msg += (selected ? "§a→ " : "  ");
                msg += "[" + std::to_string(p.getId()) + "] ";
                msg += "'" + p.getName() + "' ";
                msg += "at (" + std::to_string(pos.x) + ", "
                     + std::to_string(pos.y) + ", "
                     + std::to_string(pos.z) + ") ";
                msg += p.describeTransform();
                msg += (p.isEnabled() ? "" : " §c[DISABLED]§r");
                msg += (p.isRenderEnabled() ? "" : " §c[HIDDEN]§r");
                msg += (selected ? "§r" : "");
                msg += "\n";
            }

            output.success(msg);
        });

    // ================================================================
    // /schem remove <id>
    // ================================================================
    schemCmd.overload<SchemRemoveParam>()
        .text("remove")
        .required("id")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&       output,
                     SchemRemoveParam const& param) {
            auto* p = pm.getPlacement(static_cast<placement::SchematicPlacement::Id>(param.id));
            if (!p) {
                output.error("Placement ID {} not found.", param.id);
                return;
            }

            std::string name = p->getName();
            pm.removePlacement(static_cast<placement::SchematicPlacement::Id>(param.id));
            pm.rebuildAndRefresh(getCoordinator(origin));

            output.success("Removed placement '{}' [ID: {}]", name, param.id);
        });

    // ================================================================
    // /schem remove（无参数 = 删除当前选中）
    // ================================================================
    schemCmd.overload()
        .text("remove")
        .execute([&](CommandOrigin const& origin, CommandOutput& output) {
            auto* sel = pm.getSelected();
            if (!sel) {
                output.error("No placement selected.");
                return;
            }

            std::string name = sel->getName();
            auto id = sel->getId();
            pm.removePlacement(id);
            pm.rebuildAndRefresh(getCoordinator(origin));

            output.success("Removed placement '{}' [ID: {}]", name, id);
        });

    // ================================================================
    // /schem select <id>
    // ================================================================
    schemCmd.overload<SchemSelectParam>()
        .text("select")
        .required("id")
        .execute([&](CommandOrigin const&,
                     CommandOutput&       output,
                     SchemSelectParam const& param) {
            auto* p = pm.getPlacement(static_cast<placement::SchematicPlacement::Id>(param.id));
            if (!p) {
                output.error("Placement ID {} not found.", param.id);
                return;
            }

            pm.setSelectedId(static_cast<placement::SchematicPlacement::Id>(param.id));
            output.success("Selected placement '{}' [ID: {}]", p->getName(), param.id);
        });

    // ================================================================
    // /schem toggle render（开关渲染可见性）
    // ================================================================
    schemCmd.overload()
        .text("toggle")
        .text("render")
        .execute([&](CommandOrigin const& origin, CommandOutput& output) {
            auto* sel = pm.getSelected();
            if (!sel) {
                output.error("No placement selected.");
                return;
            }

            sel->toggleRender();
            pm.rebuildAndRefresh(getCoordinator(origin));

            output.success(
                "Render for '{}': {}§r", sel->getName(), sel->isRenderEnabled() ? "§aON" : "§cOFF");
        });

    // ================================================================
    // /schem toggle（开关 enabled 状态）
    // ================================================================
    schemCmd.overload()
        .text("toggle")
        .execute([&](CommandOrigin const& origin, CommandOutput& output) {
            auto* sel = pm.getSelected();
            if (!sel) {
                output.error("No placement selected.");
                return;
            }

            sel->toggleEnabled();
            pm.rebuildAndRefresh(getCoordinator(origin));

            output.success(
                "Placement '{}': {}§r", sel->getName(), sel->isEnabled() ? "§aENABLED" : "§cDISABLED");
        });

    // ================================================================
    // /schem info（显示当前选中投影的详细信息）
    // ================================================================
    schemCmd.overload()
        .text("info")
        .execute([&](CommandOrigin const&, CommandOutput& output) {
            auto* sel = pm.getSelected();
            if (!sel) {
                output.error("No placement selected.");
                return;
            }

            auto pos = sel->getOrigin();
            auto [minBox, maxBox] = sel->getEnclosingBox();

            std::string msg = "§ePlacement Info:§r\n";
            msg += "  Name: " + sel->getName() + "\n";
            msg += "  ID: " + std::to_string(sel->getId()) + "\n";
            msg += "  Origin: (" + std::to_string(pos.x) + ", "
                 + std::to_string(pos.y) + ", "
                 + std::to_string(pos.z) + ")\n";
            msg += "  Transform: " + sel->describeTransform() + "\n";
            msg += "  Enabled: " + std::string(sel->isEnabled() ? "Yes" : "No") + "\n";
            msg += "  Render: " + std::string(sel->isRenderEnabled() ? "Yes" : "No") + "\n";
            msg += "  Non-air blocks: " + std::to_string(sel->getTotalNonAirBlocks()) + "\n";
            msg += "  Bounding box: (" + std::to_string(minBox.x) + "," + std::to_string(minBox.y) + "," + std::to_string(minBox.z)
                 + ") to (" + std::to_string(maxBox.x) + "," + std::to_string(maxBox.y) + "," + std::to_string(maxBox.z) + ")\n";

            if (sel->hasSchematic()) {
                auto& meta = sel->getSchematic()->getMetadata();
                msg += "  Schematic: " + meta.name + "\n";
                msg += "  Author: " + meta.author + "\n";
                msg += "  Regions: " + std::to_string(meta.regionCount) + "\n";
            }

            // 子区域信息
            auto& subRegions = sel->getSubRegions();
            if (!subRegions.empty()) {
                msg += "  Sub-regions:\n";
                for (const auto& [name, sub] : subRegions) {
                    msg += "    - " + name;
                    msg += (sub.enabled ? "" : " §c[DISABLED]§r");
                    msg += "\n";
                }
            }

            output.success(msg);
        });

    // ================================================================
    // /schem files（列出可用的 .litematic 文件）
    // ================================================================
    schemCmd.overload()
        .text("files")
        .execute([&](CommandOrigin const&, CommandOutput& output) {
            auto files = pm.listAvailableFiles();
            if (files.empty()) {
                output.success(
                    "No schematic files found in: {}\nPlace .litematic or .mcstructure files there and try again.",
                    pm.getSchematicDirectory().string()
                );
                return;
            }

            std::string msg = "§eAvailable schematics (" + std::to_string(files.size()) + "):§r\n";
            for (const auto& f : files) {
                msg += "  " + f + "\n";
            }
            msg += "§7Directory: " + pm.getSchematicDirectory().string() + "§r";

            output.success(msg);
        });

    // ================================================================
    // /schem reset（重置当前投影变换）
    // ================================================================
    schemCmd.overload()
        .text("reset")
        .execute([&](CommandOrigin const& origin, CommandOutput& output) {
            auto* sel = pm.getSelected();
            if (!sel) {
                output.error("No placement selected.");
                return;
            }

            sel->resetTransform();
            pm.rebuildAndRefresh(getCoordinator(origin));

            output.success("Reset transform for '{}' → {}", sel->getName(), sel->describeTransform());
        });

    // ================================================================
    // /schem clear（清空所有投影）
    // ================================================================
    schemCmd.overload()
        .text("clear")
        .execute([&](CommandOrigin const& origin, CommandOutput& output) {
            size_t count = pm.getPlacementCount();
            auto previousSnapshot = dm.getProjectionState().getSnapshot();
            pm.removeAllPlacements();
            dm.getProjectionState().clear();
            sTestEntries.clear();
            render::triggerRebuildForProjection(getCoordinator(origin), std::move(previousSnapshot));

            output.success("Cleared {} placement(s).", count);
        });

    // ================================================================
    // Phase 4: 选区命令
    // ================================================================

    // /schem pos1 [x y z] — 设置选区角点1
    schemCmd.overload<SchemOriginParam>()
        .text("pos1")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&       output,
                     SchemOriginParam const& param,
                     Command const&       cmd) {
            auto& selMgr = dm.getSelectionManager();
            BlockPos pos(origin.getExecutePosition(cmd.mVersion, param.pos));
            selMgr.setCorner1(pos);
            output.success("Selection corner 1 set to ({})", pos.toString());
        });

    // /schem pos1（无坐标 = 玩家位置）
    schemCmd.overload()
        .text("pos1")
        .execute([&](CommandOrigin const& origin, CommandOutput& output) {
            auto& selMgr = dm.getSelectionManager();
            BlockPos pos(origin.getWorldPosition());
            selMgr.setCorner1(pos);
            output.success("Selection corner 1 set to player position ({})", pos.toString());
        });

    // /schem pos2 [x y z] — 设置选区角点2
    schemCmd.overload<SchemOriginParam>()
        .text("pos2")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&       output,
                     SchemOriginParam const& param,
                     Command const&       cmd) {
            auto& selMgr = dm.getSelectionManager();
            BlockPos pos(origin.getExecutePosition(cmd.mVersion, param.pos));
            selMgr.setCorner2(pos);
            output.success("Selection corner 2 set to ({})", pos.toString());
        });

    // /schem pos2（无坐标 = 玩家位置）
    schemCmd.overload()
        .text("pos2")
        .execute([&](CommandOrigin const& origin, CommandOutput& output) {
            auto& selMgr = dm.getSelectionManager();
            BlockPos pos(origin.getWorldPosition());
            selMgr.setCorner2(pos);
            output.success("Selection corner 2 set to player position ({})", pos.toString());
        });

    // /schem save <name> — 将选区保存为 .mcstructure 文件
    schemCmd.overload<SchemLoadSimpleParam>()
        .text("save")
        .required("filename")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&       output,
                     SchemLoadSimpleParam const& param) {
            auto& selMgr = dm.getSelectionManager();
            if (!selMgr.hasCompleteSelection()) {
                output.error("Selection is incomplete. Set both corners first (pos1/pos2).");
                return;
            }

            auto* dim = origin.getDimension();
            if (!dim) {
                output.error("Cannot determine current dimension.");
                return;
            }

            bool success = selMgr.saveToMcstructure(param.filename, *dim);
            if (success) {
                auto size = selMgr.getSize();
                output.success(
                    "Saved selection as '{}' ({}x{}x{})",
                    param.filename,
                    size.x,
                    size.y,
                    size.z
                );
            } else {
                output.error("Failed to save selection as '{}'", param.filename);
            }
        });

    // /schem selection clear — 清除选区
    schemCmd.overload()
        .text("selection")
        .text("clear")
        .execute([&](CommandOrigin const&, CommandOutput& output) {
            dm.getSelectionManager().clearSelection();
            output.success("Selection cleared.");
        });

    // /schem selection mode — 开关选区模式
    schemCmd.overload()
        .text("selection")
        .text("mode")
        .execute([&](CommandOrigin const&, CommandOutput& output) {
            auto& selMgr = dm.getSelectionManager();
            selMgr.toggleSelectionMode();
            output.success(
                "Selection mode: {}§r",
                selMgr.isSelectionMode() ? "§aON" : "§cOFF"
            );
        });

    // /schem selection info — 显示选区信息
    schemCmd.overload()
        .text("selection")
        .text("info")
        .execute([&](CommandOrigin const&, CommandOutput& output) {
            auto& selMgr = dm.getSelectionManager();
            std::string msg = "§eSelection Info:§r\n";
            msg += "  Mode: " + std::string(selMgr.isSelectionMode() ? "§aON§r" : "§cOFF§r") + "\n";

            if (selMgr.hasCorner1()) {
                auto c1 = selMgr.getCorner1();
                msg += "  Corner 1: (" + std::to_string(c1.x) + ", "
                     + std::to_string(c1.y) + ", "
                     + std::to_string(c1.z) + ")\n";
            } else {
                msg += "  Corner 1: §7(not set)§r\n";
            }

            if (selMgr.hasCorner2()) {
                auto c2 = selMgr.getCorner2();
                msg += "  Corner 2: (" + std::to_string(c2.x) + ", "
                     + std::to_string(c2.y) + ", "
                     + std::to_string(c2.z) + ")\n";
            } else {
                msg += "  Corner 2: §7(not set)§r\n";
            }

            if (selMgr.hasCompleteSelection()) {
                auto size = selMgr.getSize();
                auto minC = selMgr.getMinCorner();
                msg += "  Size: " + std::to_string(size.x) + "x"
                     + std::to_string(size.y) + "x"
                     + std::to_string(size.z) + "\n";
                msg += "  Min corner: (" + std::to_string(minC.x) + ", "
                     + std::to_string(minC.y) + ", "
                     + std::to_string(minC.z) + ")\n";
                msg += "  Total blocks: " + std::to_string(
                    static_cast<int64_t>(size.x) * size.y * size.z) + "\n";
            }

            output.success(msg);
        });

    // ================================================================
    // /schem（无子命令 = 显示帮助）
    // ================================================================
    schemCmd.overload()
        .execute([&](CommandOrigin const&, CommandOutput& output) {
            std::string help = "§eLeviSchematic Commands:§r\n";
            help += "  §a/schem load <file> [x y z]§r - Load schematic\n";
            help += "  §a/schem move <dx> <dy> <dz>§r - Move placement\n";
            help += "  §a/schem origin <x> <y> <z>§r - Set origin\n";
            help += "  §a/schem rotate <cw90|ccw90|r180>§r - Rotate\n";
            help += "  §a/schem mirror <x|z|none>§r - Mirror\n";
            help += "  §a/schem list§r - List placements\n";
            help += "  §a/schem select <id>§r - Select placement\n";
            help += "  §a/schem remove [id]§r - Remove placement\n";
            help += "  §a/schem toggle [render]§r - Toggle on/off\n";
            help += "  §a/schem info§r - Show placement info\n";
            help += "  §a/schem files§r - List available files\n";
            help += "  §a/schem reset§r - Reset transform\n";
            help += "  §a/schem clear§r - Clear all\n";
            help += "  §a/schem pos1 [x y z]§r - Set selection corner 1\n";
            help += "  §a/schem pos2 [x y z]§r - Set selection corner 2\n";
            help += "  §a/schem save <name>§r - Save selection as .mcstructure\n";
            help += "  §a/schem selection clear§r - Clear selection\n";
            help += "  §a/schem selection mode§r - Toggle selection mode\n";
            help += "  §a/schem selection info§r - Show selection info\n";
            output.success(help);
        });

    getLogger().info("LeviSchematic commands registered (Phase 4)");
}

} // namespace levishematic::command
