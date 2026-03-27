#include "levishematic/command/CommandShared.h"

#include "mc/server/commands/Command.h"
#include "mc/world/level/BlockPos.h"

namespace levishematic::command {

void registerSchemLoadCommands(ll::command::CommandHandle& schemCmd) {
    schemCmd.overload<SchemLoadParam>()
        .text("load")
        .required("filename")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemLoadParam const& param,
                     Command const&      cmd) {
            auto& controller = app::getAppKernel().controller();
            auto  blockPos   = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
            auto  result     = controller.loadSchematic(param.filename, blockPos);
            if (!result) {
                replyLoadFailure(output, param.filename, result);
                return;
            }

            auto const* placement = controller.findPlacement(*result.id);
            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success(
                    "Loaded '{}' at ({}) [ID: {}]",
                    placement ? placement->name : param.filename,
                    blockPos.toString(),
                    *result.id
                );
            });
        });

    schemCmd.overload<SchemNamedParam>()
        .text("load")
        .required("filename")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemNamedParam const& param) {
            auto& controller = app::getAppKernel().controller();
            auto  blockPos   = BlockPos(origin.getWorldPosition());
            auto  result     = controller.loadSchematic(param.filename, blockPos);
            if (!result) {
                replyLoadFailure(output, param.filename, result);
                return;
            }

            auto const* placement = controller.findPlacement(*result.id);
            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success(
                    "Loaded '{}' at player position ({}) [ID: {}]",
                    placement ? placement->name : param.filename,
                    blockPos.toString(),
                    *result.id
                );
            });
        });

    schemCmd.overload().text("files").execute([&](CommandOrigin const&, CommandOutput& output) {
        auto const& store = app::getAppKernel().controller().placementStore();
        auto        files = store.listAvailableFiles();
        if (files.empty()) {
            output.success(
                "No .mcstructure files found in: {}\nPlace .mcstructure files there and try again.",
                store.schematicDirectory().string()
            );
            return;
        }

        std::string msg = "Available schematics (" + std::to_string(files.size()) + "):\n";
        for (auto const& file : files) {
            msg += "  " + file + "\n";
        }
        msg += "Directory: " + store.schematicDirectory().string();

        output.success(msg);
    });
}

} // namespace levishematic::command
