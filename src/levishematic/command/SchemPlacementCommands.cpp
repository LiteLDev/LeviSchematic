#include "levishematic/command/CommandShared.h"

#include "levishematic/core/DataManager.h"

namespace levishematic::command {

void registerSchemPlacementCommands(ll::command::CommandHandle& schemCmd) {
    auto& dm = core::DataManager::getInstance();
    auto& pm = dm.getPlacementManager();

    schemCmd.overload().text("list").execute([&](CommandOrigin const&, CommandOutput& output) {
        auto& placements = pm.getAllPlacements();
        if (placements.empty()) {
            output.success("No placements loaded.");
            return;
        }

        std::string msg = "Loaded placements (" + std::to_string(placements.size()) + "):\n";
        for (const auto& placement : placements) {
            auto pos     = placement.getOrigin();
            auto current = placement.getId() == pm.getSelectedId();

            msg += current ? "-> " : "   ";
            msg += "[" + std::to_string(placement.getId()) + "] ";
            msg += "'" + placement.getName() + "' ";
            msg += "at (" + std::to_string(pos.x) + ", "
                + std::to_string(pos.y) + ", "
                + std::to_string(pos.z) + ") ";
            msg += placement.describeTransform();
            msg += placement.isEnabled() ? "" : " [DISABLED]";
            msg += placement.isRenderEnabled() ? "" : " [HIDDEN]";
            msg += "\n";
        }

        output.success(msg);
    });

    schemCmd.overload<SchemRemoveParam>()
        .text("remove")
        .required("id")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemRemoveParam const& param) {
            auto* placement = pm.getPlacement(static_cast<placement::SchematicPlacement::Id>(param.id));
            if (!placement) {
                logPlacementCommandFailure(
                    "command.removePlacement",
                    {},
                    static_cast<placement::SchematicPlacement::Id>(param.id),
                    "placement not found"
                );
                output.error("Placement ID {} not found.", param.id);
                return;
            }

            auto name = placement->getName();
            pm.removePlacement(static_cast<placement::SchematicPlacement::Id>(param.id));
            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success("Removed placement '{}' [ID: {}]", name, param.id);
            });
        });

    schemCmd.overload().text("remove").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        withSelectedPlacement(pm, output, [&](placement::SchematicPlacement& selected) {
            auto name = selected.getName();
            auto id   = selected.getId();
            pm.removePlacement(id);
            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success("Removed placement '{}' [ID: {}]", name, id);
            });
        });
    });

    schemCmd.overload<SchemSelectParam>()
        .text("select")
        .required("id")
        .execute([&](CommandOrigin const&, CommandOutput& output, SchemSelectParam const& param) {
            auto* placement = pm.getPlacement(static_cast<placement::SchematicPlacement::Id>(param.id));
            if (!placement) {
                logPlacementCommandFailure(
                    "command.selectPlacement",
                    {},
                    static_cast<placement::SchematicPlacement::Id>(param.id),
                    "placement not found"
                );
                output.error("Placement ID {} not found.", param.id);
                return;
            }

            pm.setSelectedId(static_cast<placement::SchematicPlacement::Id>(param.id));
            output.success("Selected placement '{}' [ID: {}]", placement->getName(), param.id);
        });

    schemCmd.overload().text("toggle").text("render").execute(
        [&](CommandOrigin const& origin, CommandOutput& output) {
            withSelectedPlacement(pm, output, [&](placement::SchematicPlacement& selected) {
                selected.toggleRender();
                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success(
                        "Render for '{}': {}",
                        selected.getName(),
                        selected.isRenderEnabled() ? "ON" : "OFF"
                    );
                });
            });
        }
    );

    schemCmd.overload().text("toggle").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        withSelectedPlacement(pm, output, [&](placement::SchematicPlacement& selected) {
            selected.toggleEnabled();
            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success(
                    "Placement '{}': {}",
                    selected.getName(),
                    selected.isEnabled() ? "ENABLED" : "DISABLED"
                );
            });
        });
    });

    schemCmd.overload().text("info").execute([&](CommandOrigin const&, CommandOutput& output) {
        withSelectedPlacement(pm, output, [&](placement::SchematicPlacement& selected) {
            auto pos              = selected.getOrigin();
            auto [minBox, maxBox] = selected.getEnclosingBox();

            std::string msg = "Placement Info:\n";
            msg += "  Name: " + selected.getName() + "\n";
            msg += "  ID: " + std::to_string(selected.getId()) + "\n";
            msg += "  Origin: (" + std::to_string(pos.x) + ", "
                + std::to_string(pos.y) + ", "
                + std::to_string(pos.z) + ")\n";
            msg += "  Transform: " + selected.describeTransform() + "\n";
            msg += "  Enabled: " + std::string(selected.isEnabled() ? "Yes" : "No") + "\n";
            msg += "  Render: " + std::string(selected.isRenderEnabled() ? "Yes" : "No") + "\n";
            msg += "  Non-air blocks: " + std::to_string(selected.getTotalNonAirBlocks()) + "\n";
            msg += "  Bounding box: (" + std::to_string(minBox.x) + ","
                + std::to_string(minBox.y) + ","
                + std::to_string(minBox.z) + ") to ("
                + std::to_string(maxBox.x) + ","
                + std::to_string(maxBox.y) + ","
                + std::to_string(maxBox.z) + ")\n";

            output.success(msg);
        });
    });

    schemCmd.overload().text("clear").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        auto count    = pm.getPlacementCount();
        pm.removeAllPlacements();
        clearRenderTestEntries();
        flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
            out.success("Cleared {} placement(s).", count);
        });
    });

    schemCmd.overload().execute([&](CommandOrigin const&, CommandOutput& output) {
        std::string help = "LeviSchematic Commands:\n";
        help += "  /schem load <file> [x y z] - Load schematic\n";
        help += "  /schem move <dx> <dy> <dz> - Move placement\n";
        help += "  /schem origin <x> <y> <z> - Set origin\n";
        help += "  /schem rotate <cw90|ccw90|r180> - Rotate\n";
        help += "  /schem mirror <x|z|none> - Mirror\n";
        help += "  /schem list - List placements\n";
        help += "  /schem select <id> - Select placement\n";
        help += "  /schem remove [id] - Remove placement\n";
        help += "  /schem toggle [render] - Toggle placement state\n";
        help += "  /schem info - Show placement info\n";
        help += "  /schem files - List available files\n";
        help += "  /schem reset - Reset transform\n";
        help += "  /schem clear - Clear all\n";
        help += "  /schem pos1 [x y z] - Set selection corner 1\n";
        help += "  /schem pos2 [x y z] - Set selection corner 2\n";
        help += "  /schem save <name> - Save selection as .mcstructure\n";
        help += "  /schem selection clear - Clear selection\n";
        help += "  /schem selection mode - Toggle selection mode\n";
        help += "  /schem selection info - Show selection info\n";
        output.success(help);
    });
}

} // namespace levishematic::command
