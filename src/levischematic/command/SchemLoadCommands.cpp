#include "levischematic/command/CommandShared.h"

#include "mc/server/commands/Command.h"
#include "mc/world/level/BlockPos.h"

namespace levischematic::command {

void registerSchemLoadCommands(ll::command::CommandHandle& schemCmd) {
    schemCmd.overload<SchemLoadParam>()
        .text("load")
        .required("filename")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemLoadParam const& param,
                     Command const&      cmd) {
            auto& placementService = app::getAppKernel().placement();
            auto  blockPos         = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
            auto* dimension        = origin.getDimension();
            auto  result           = placementService.loadSchematic(
                param.filename,
                blockPos,
                dimension ? static_cast<int>(dimension->getDimensionId()) : 0
            );
            if (!result) {
                replyPlacementError(output, "command.loadSchematic", result.error());
                return;
            }

            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success(
                    "Loaded '{}' at ({}) [ID: {}]",
                    result.value().name,
                    blockPos.toString(),
                    result.value().id
                );
            });
        });

    schemCmd.overload<SchemNamedParam>()
        .text("load")
        .required("filename")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemNamedParam const& param) {
            auto& placementService = app::getAppKernel().placement();
            auto  blockPos         = BlockPos(origin.getWorldPosition());
            auto* dimension        = origin.getDimension();
            auto  result           = placementService.loadSchematic(
                param.filename,
                blockPos,
                dimension ? static_cast<int>(dimension->getDimensionId()) : 0
            );
            if (!result) {
                replyPlacementError(output, "command.loadSchematic", result.error());
                return;
            }

            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success(
                    "Loaded '{}' at player position ({}) [ID: {}]",
                    result.value().name,
                    blockPos.toString(),
                    result.value().id
                );
            });
        });

    schemCmd.overload().text("files").execute([&](CommandOrigin const&, CommandOutput& output) {
        auto const& runtime = app::getAppKernel().runtime();
        auto        files   = runtime.listAvailableSchematics();
        if (files.empty()) {
            output.success(
                "No .mcstructure files found in: {}\nPlace .mcstructure files there and try again.",
                runtime.schematicDirectory().string()
            );
            return;
        }

        std::string msg = "Available schematics (" + std::to_string(files.size()) + "):\n";
        for (auto const& file : files) {
            msg += "  " + file + "\n";
        }
        msg += "Directory: " + runtime.schematicDirectory().string();

        output.success(msg);
    });
}

} // namespace levischematic::command
