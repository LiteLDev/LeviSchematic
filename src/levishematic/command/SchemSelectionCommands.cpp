#include "levishematic/command/CommandShared.h"

#include "levishematic/core/DataManager.h"

#include "mc/server/commands/Command.h"
#include "mc/world/level/BlockPos.h"

namespace levishematic::command {

void registerSchemSelectionCommands(ll::command::CommandHandle& schemCmd) {
    auto& dm     = core::DataManager::getInstance();
    auto& selMgr = dm.getSelectionManager();

    schemCmd.overload<SchemOriginParam>()
        .text("pos1")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemOriginParam const& param,
                     Command const&      cmd) {
            auto blockPos = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
            selMgr.setCorner1(blockPos);
            output.success("Selection corner 1 set to ({})", blockPos.toString());
        });

    schemCmd.overload().text("pos1").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        auto blockPos = BlockPos(origin.getWorldPosition());
        selMgr.setCorner1(blockPos);
        output.success("Selection corner 1 set to player position ({})", blockPos.toString());
    });

    schemCmd.overload<SchemOriginParam>()
        .text("pos2")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemOriginParam const& param,
                     Command const&      cmd) {
            auto blockPos = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
            selMgr.setCorner2(blockPos);
            output.success("Selection corner 2 set to ({})", blockPos.toString());
        });

    schemCmd.overload().text("pos2").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        auto blockPos = BlockPos(origin.getWorldPosition());
        selMgr.setCorner2(blockPos);
        output.success("Selection corner 2 set to player position ({})", blockPos.toString());
    });

    schemCmd.overload<SchemNamedParam>()
        .text("save")
        .required("filename")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemNamedParam const& param) {
            if (!selMgr.hasCompleteSelection()) {
                logPlacementCommandFailure("command.saveSelection", param.filename, 0, "selection is incomplete");
                output.error("Selection is incomplete. Set both corners first (pos1/pos2).");
                return;
            }

            auto* dimension = origin.getDimension();
            if (!dimension) {
                logPlacementCommandFailure("command.saveSelection", param.filename, 0, "dimension unavailable");
                output.error("Cannot determine current dimension.");
                return;
            }

            if (!selMgr.saveToMcstructure(param.filename, *dimension)) {
                logPlacementCommandFailure("command.saveSelection", param.filename, 0, "saveToMcstructure returned false");
                output.error("Failed to save selection as '{}'", param.filename);
                return;
            }

            auto size = selMgr.getSize();
            output.success(
                "Saved selection as '{}' ({}x{}x{})",
                param.filename,
                size.x,
                size.y,
                size.z
            );
        });

    schemCmd.overload().text("selection").text("clear").execute([&](CommandOrigin const&, CommandOutput& output) {
        selMgr.clearSelection();
        output.success("Selection cleared.");
    });

    schemCmd.overload().text("selection").text("mode").execute([&](CommandOrigin const&, CommandOutput& output) {
        selMgr.toggleSelectionMode();
        output.success("Selection mode: {}", selMgr.isSelectionMode() ? "ON" : "OFF");
    });

    schemCmd.overload().text("selection").text("info").execute([&](CommandOrigin const&, CommandOutput& output) {
        std::string msg = "Selection Info:\n";
        msg += "  Mode: " + std::string(selMgr.isSelectionMode() ? "ON" : "OFF") + "\n";

        if (selMgr.hasCorner1()) {
            auto corner1 = selMgr.getCorner1();
            msg += "  Corner 1: (" + std::to_string(corner1.x) + ", "
                + std::to_string(corner1.y) + ", "
                + std::to_string(corner1.z) + ")\n";
        } else {
            msg += "  Corner 1: (not set)\n";
        }

        if (selMgr.hasCorner2()) {
            auto corner2 = selMgr.getCorner2();
            msg += "  Corner 2: (" + std::to_string(corner2.x) + ", "
                + std::to_string(corner2.y) + ", "
                + std::to_string(corner2.z) + ")\n";
        } else {
            msg += "  Corner 2: (not set)\n";
        }

        if (selMgr.hasCompleteSelection()) {
            auto size      = selMgr.getSize();
            auto minCorner = selMgr.getMinCorner();
            msg += "  Size: " + std::to_string(size.x) + "x"
                + std::to_string(size.y) + "x"
                + std::to_string(size.z) + "\n";
            msg += "  Min corner: (" + std::to_string(minCorner.x) + ", "
                + std::to_string(minCorner.y) + ", "
                + std::to_string(minCorner.z) + ")\n";
            msg += "  Total blocks: "
                + std::to_string(static_cast<int64_t>(size.x) * size.y * size.z) + "\n";
        }

        output.success(msg);
    });
}

} // namespace levishematic::command
