#include "levishematic/command/CommandShared.h"

#include "levishematic/selection/SelectionExporter.h"

#include "mc/server/commands/Command.h"
#include "mc/world/level/BlockPos.h"

namespace levishematic::command {

void registerSchemSelectionCommands(ll::command::CommandHandle& schemCmd) {
    schemCmd.overload<SchemOriginParam>()
        .text("pos1")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemOriginParam const& param,
                     Command const&      cmd) {
            auto& controller = app::getAppKernel().controller();
            auto  blockPos   = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
            controller.setSelectionCorner1(blockPos);
            output.success("Selection corner 1 set to ({})", blockPos.toString());
        });

    schemCmd.overload().text("pos1").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        auto& controller = app::getAppKernel().controller();
        auto  blockPos   = BlockPos(origin.getWorldPosition());
        controller.setSelectionCorner1(blockPos);
        output.success("Selection corner 1 set to player position ({})", blockPos.toString());
    });

    schemCmd.overload<SchemOriginParam>()
        .text("pos2")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemOriginParam const& param,
                     Command const&      cmd) {
            auto& controller = app::getAppKernel().controller();
            auto  blockPos   = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
            controller.setSelectionCorner2(blockPos);
            output.success("Selection corner 2 set to ({})", blockPos.toString());
        });

    schemCmd.overload().text("pos2").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        auto& controller = app::getAppKernel().controller();
        auto  blockPos   = BlockPos(origin.getWorldPosition());
        controller.setSelectionCorner2(blockPos);
        output.success("Selection corner 2 set to player position ({})", blockPos.toString());
    });

    schemCmd.overload<SchemNamedParam>()
        .text("save")
        .required("filename")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemNamedParam const& param) {
            auto& selectionState = app::getAppKernel().controller().state().selection;
            if (!selection::hasCompleteSelection(selectionState)) {
                logPlacementCommandFailure("command.saveSelection", param.filename, std::nullopt, "selection is incomplete");
                output.error("Selection is incomplete. Set both corners first (pos1/pos2).");
                return;
            }

            auto* dimension = origin.getDimension();
            if (!dimension) {
                logPlacementCommandFailure("command.saveSelection", param.filename, std::nullopt, "dimension unavailable");
                output.error("Cannot determine current dimension.");
                return;
            }

            if (!app::getAppKernel().controller().saveSelection(param.filename, *dimension)) {
                logPlacementCommandFailure("command.saveSelection", param.filename, std::nullopt, "saveSelection returned false");
                output.error("Failed to save selection as '{}'", param.filename);
                return;
            }

            auto size = selection::getSize(selectionState);
            output.success(
                "Saved selection as '{}' ({}x{}x{})",
                param.filename,
                size.x,
                size.y,
                size.z
            );
        });

    schemCmd.overload().text("selection").text("clear").execute([&](CommandOrigin const&, CommandOutput& output) {
        app::getAppKernel().controller().clearSelection();
        output.success("Selection cleared.");
    });

    schemCmd.overload().text("selection").text("mode").execute([&](CommandOrigin const&, CommandOutput& output) {
        auto& controller = app::getAppKernel().controller();
        controller.toggleSelectionMode();
        output.success("Selection mode: {}", controller.state().selection.selectionMode ? "ON" : "OFF");
    });

    schemCmd.overload().text("selection").text("info").execute([&](CommandOrigin const&, CommandOutput& output) {
        auto const& selectionState = app::getAppKernel().controller().state().selection;
        std::string msg            = "Selection Info:\n";
        msg += "  Mode: " + std::string(selectionState.selectionMode ? "ON" : "OFF") + "\n";

        if (selectionState.corner1) {
            auto const& corner1 = *selectionState.corner1;
            msg += "  Corner 1: (" + std::to_string(corner1.x) + ", "
                + std::to_string(corner1.y) + ", "
                + std::to_string(corner1.z) + ")\n";
        } else {
            msg += "  Corner 1: (not set)\n";
        }

        if (selectionState.corner2) {
            auto const& corner2 = *selectionState.corner2;
            msg += "  Corner 2: (" + std::to_string(corner2.x) + ", "
                + std::to_string(corner2.y) + ", "
                + std::to_string(corner2.z) + ")\n";
        } else {
            msg += "  Corner 2: (not set)\n";
        }

        if (selection::hasCompleteSelection(selectionState)) {
            auto size      = selection::getSize(selectionState);
            auto minCorner = selection::getMinCorner(selectionState);
            msg += "  Size: " + std::to_string(size.x) + "x"
                + std::to_string(size.y) + "x"
                + std::to_string(size.z) + "\n";
            msg += "  Min corner: (" + std::to_string(minCorner.x) + ", "
                + std::to_string(minCorner.y) + ", "
                + std::to_string(minCorner.z) + ")\n";
            msg += "  Total blocks: " + std::to_string(static_cast<int64_t>(size.x) * size.y * size.z) + "\n";
        }

        output.success(msg);
    });
}

} // namespace levishematic::command
