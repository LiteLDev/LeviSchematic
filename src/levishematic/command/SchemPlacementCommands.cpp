#include "levishematic/command/CommandShared.h"

#include "levishematic/render/ProjectionRenderer.h"
#include "levishematic/schematic/placement/PlacementModel.h"

#include "mc/server/commands/Command.h"
#include "mc/server/commands/CommandBlockNameResult.h"

namespace levishematic::command {

void registerSchemPlacementCommands(ll::command::CommandHandle& schemCmd) {
    schemCmd.overload().text("list").execute([&](CommandOrigin const&, CommandOutput& output) {
        auto& controller = app::getAppKernel().controller();
        auto  placements = controller.orderedPlacements();
        if (placements.empty()) {
            output.success("No placements loaded.");
            return;
        }

        std::string msg = "Loaded placements (" + std::to_string(placements.size()) + "):\n";
        auto selectedId = controller.selectedPlacementId();
        for (auto const& placementRef : placements) {
            auto const& placement = placementRef.get();
            auto const& pos       = placement.origin;
            bool        current   = selectedId && *selectedId == placement.id;

            msg += current ? "-> " : "   ";
            msg += "[" + std::to_string(placement.id) + "] ";
            msg += "'" + placement.name + "' ";
            msg += "at (" + std::to_string(pos.x) + ", "
                + std::to_string(pos.y) + ", "
                + std::to_string(pos.z) + ") ";
            msg += placement.describeTransform();
            msg += placement.enabled ? "" : " [DISABLED]";
            msg += placement.renderEnabled ? "" : " [HIDDEN]";
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
            auto& controller = app::getAppKernel().controller();
            auto const* placement = controller.findPlacement(static_cast<placement::PlacementId>(param.id));
            if (!placement) {
                logPlacementCommandFailure("command.removePlacement", {}, static_cast<placement::PlacementId>(param.id), "placement not found");
                output.error("Placement ID {} not found.", param.id);
                return;
            }

            auto name = placement->name;
            controller.removePlacement(static_cast<placement::PlacementId>(param.id));
            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success("Removed placement '{}' [ID: {}]", name, param.id);
            });
        });

    schemCmd.overload().text("remove").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        auto& controller = app::getAppKernel().controller();
        withSelectedPlacement(controller, output, [&](placement::PlacementInstance const& selected) {
            auto name = selected.name;
            auto id   = selected.id;
            controller.removePlacement(id);
            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success("Removed placement '{}' [ID: {}]", name, id);
            });
        });
    });

    schemCmd.overload<SchemSelectParam>()
        .text("select")
        .required("id")
        .execute([&](CommandOrigin const&, CommandOutput& output, SchemSelectParam const& param) {
            auto& controller = app::getAppKernel().controller();
            auto const* placement = controller.findPlacement(static_cast<placement::PlacementId>(param.id));
            if (!placement) {
                logPlacementCommandFailure("command.selectPlacement", {}, static_cast<placement::PlacementId>(param.id), "placement not found");
                output.error("Placement ID {} not found.", param.id);
                return;
            }

            controller.selectPlacement(static_cast<placement::PlacementId>(param.id));
            output.success("Selected placement '{}' [ID: {}]", placement->name, param.id);
        });

    schemCmd.overload().text("toggle").text("render").execute(
        [&](CommandOrigin const& origin, CommandOutput& output) {
            auto& controller = app::getAppKernel().controller();
            withSelectedPlacement(controller, output, [&](placement::PlacementInstance const& selected) {
                controller.togglePlacementRender(selected.id);
                auto const* updated = controller.findPlacement(selected.id);
                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success(
                        "Render for '{}': {}",
                        selected.name,
                        updated && updated->renderEnabled ? "ON" : "OFF"
                    );
                });
            });
        }
    );

    schemCmd.overload().text("toggle").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        auto& controller = app::getAppKernel().controller();
        withSelectedPlacement(controller, output, [&](placement::PlacementInstance const& selected) {
            controller.togglePlacementEnabled(selected.id);
            auto const* updated = controller.findPlacement(selected.id);
            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success(
                    "Placement '{}': {}",
                    selected.name,
                    updated && updated->enabled ? "ENABLED" : "DISABLED"
                );
            });
        });
    });

    schemCmd.overload<SchemBlockPosParam>()
        .text("block")
        .text("hide")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemBlockPosParam const& param,
                     Command const&      cmd) {
            auto& controller = app::getAppKernel().controller();
            withSelectedPlacement(controller, output, [&](placement::PlacementInstance const& selected) {
                auto worldPos = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                if (!controller.patchPlacementBlock(selected.id, worldPos, render::PatchOp::remove())) {
                    output.error("Failed to hide block at ({}) for '{}'.", worldPos.toString(), selected.name);
                    return;
                }

                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success("Hidden block at ({}) for '{}'.", worldPos.toString(), selected.name);
                });
            });
        });

    schemCmd.overload<SchemBlockSetParam>()
        .text("block")
        .text("set")
        .required("pos")
        .required("blockName")
        .required("title_date")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemBlockSetParam const& param,
                     Command const&      cmd) {
            auto& controller = app::getAppKernel().controller();
            withSelectedPlacement(controller, output, [&](placement::PlacementInstance const& selected) {
                auto worldPos    = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                auto blockResult = param.blockName.resolveBlock(param.title_date);
                if (!blockResult.mBlock) {
                    output.error("Failed to resolve block '{}'.", param.blockName.getBlockName());
                    return;
                }

                if (!controller.patchPlacementBlock(
                        selected.id,
                        worldPos,
                        render::PatchOp::setBlock(blockResult.mBlock, render::kDefaultProjectionColor)
                    )) {
                    output.error("Failed to set block at ({}) for '{}'.", worldPos.toString(), selected.name);
                    return;
                }

                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success(
                        "Set block at ({}) for '{}' to '{}'.",
                        worldPos.toString(),
                        selected.name,
                        param.blockName.getBlockName()
                    );
                });
            });
        });

    schemCmd.overload<SchemBlockSetSimpleParam>()
        .text("block")
        .text("set")
        .required("pos")
        .required("blockName")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemBlockSetSimpleParam const& param,
                     Command const&      cmd) {
            auto& controller = app::getAppKernel().controller();
            withSelectedPlacement(controller, output, [&](placement::PlacementInstance const& selected) {
                auto worldPos    = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                auto blockResult = param.blockName.resolveBlock(0);
                if (!blockResult.mBlock) {
                    output.error("Failed to resolve block '{}'.", param.blockName.getBlockName());
                    return;
                }

                if (!controller.patchPlacementBlock(
                        selected.id,
                        worldPos,
                        render::PatchOp::setBlock(blockResult.mBlock, render::kDefaultProjectionColor)
                    )) {
                    output.error("Failed to set block at ({}) for '{}'.", worldPos.toString(), selected.name);
                    return;
                }

                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success(
                        "Set block at ({}) for '{}' to '{}'.",
                        worldPos.toString(),
                        selected.name,
                        param.blockName.getBlockName()
                    );
                });
            });
        });

    schemCmd.overload<SchemBlockPosParam>()
        .text("block")
        .text("clear")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemBlockPosParam const& param,
                     Command const&      cmd) {
            auto& controller = app::getAppKernel().controller();
            withSelectedPlacement(controller, output, [&](placement::PlacementInstance const& selected) {
                auto worldPos = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                if (!controller.patchPlacementBlock(selected.id, worldPos, render::PatchOp::clearOverride())) {
                    output.error("No override found at ({}) for '{}'.", worldPos.toString(), selected.name);
                    return;
                }

                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success("Cleared block override at ({}) for '{}'.", worldPos.toString(), selected.name);
                });
            });
        });

    schemCmd.overload().text("info").execute([&](CommandOrigin const&, CommandOutput& output) {
        auto& controller = app::getAppKernel().controller();
        withSelectedPlacement(controller, output, [&](placement::PlacementInstance const& selected) {
            auto pos              = selected.origin;
            auto [minBox, maxBox] = placement::computeEnclosingBox(selected);

            std::string msg = "Placement Info:\n";
            msg += "  Name: " + selected.name + "\n";
            msg += "  ID: " + std::to_string(selected.id) + "\n";
            msg += "  Origin: (" + std::to_string(pos.x) + ", "
                + std::to_string(pos.y) + ", "
                + std::to_string(pos.z) + ")\n";
            msg += "  Transform: " + selected.describeTransform() + "\n";
            msg += "  Enabled: " + std::string(selected.enabled ? "Yes" : "No") + "\n";
            msg += "  Render: " + std::string(selected.renderEnabled ? "Yes" : "No") + "\n";
            msg += "  Non-air blocks: " + std::to_string(selected.totalNonAirBlocks()) + "\n";
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
        auto& controller = app::getAppKernel().controller();
        auto  count      = controller.orderedPlacements().size();
        controller.clearPlacements();
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
        help += "  /schem block hide <x y z> - Hide a projected block\n";
        help += "  /schem block set <x y z> <block> - Override one projected block\n";
        help += "  /schem block clear <x y z> - Clear one block override\n";
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
