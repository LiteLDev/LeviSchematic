#include "levishematic/command/CommandShared.h"

#include "levishematic/core/DataManager.h"

#include "mc/server/commands/Command.h"
#include "mc/world/level/BlockPos.h"

namespace levishematic::command {

void registerSchemLoadCommands(ll::command::CommandHandle& schemCmd) {
    auto& dm = core::DataManager::getInstance();
    auto& pm = dm.getPlacementManager();

    schemCmd.overload<SchemLoadParam>()
        .text("load")
        .required("filename")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemLoadParam const& param,
                     Command const&      cmd) {
            auto resolvedPath = pm.resolveSchematicPath(param.filename);
            auto blockPos     = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
            auto result       = pm.loadAndPlaceDetailed(resolvedPath, blockPos);
            if (!result) {
                replyLoadFailure(output, param.filename, result);
                return;
            }

            auto* placement = pm.getPlacement(result.id);
            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success(
                    "Loaded '{}' at ({}) [ID: {}]",
                    placement ? placement->getName() : param.filename,
                    blockPos.toString(),
                    result.id
                );
            });
        });

    schemCmd.overload<SchemNamedParam>()
        .text("load")
        .required("filename")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemNamedParam const& param) {
            auto resolvedPath = pm.resolveSchematicPath(param.filename);
            auto blockPos     = BlockPos(origin.getWorldPosition());
            auto result       = pm.loadAndPlaceDetailed(resolvedPath, blockPos);
            if (!result) {
                replyLoadFailure(output, param.filename, result);
                return;
            }

            auto* placement = pm.getPlacement(result.id);
            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success(
                    "Loaded '{}' at player position ({}) [ID: {}]",
                    placement ? placement->getName() : param.filename,
                    blockPos.toString(),
                    result.id
                );
            });
        });

    schemCmd.overload().text("files").execute([&](CommandOrigin const&, CommandOutput& output) {
        auto files = pm.listAvailableFiles();
        if (files.empty()) {
            output.success(
                "No .mcstructure files found in: {}\nPlace .mcstructure files there and try again.",
                pm.getSchematicDirectory().string()
            );
            return;
        }

        std::string msg = "Available schematics (" + std::to_string(files.size()) + "):\n";
        for (const auto& file : files) {
            msg += "  " + file + "\n";
        }
        msg += "Directory: " + pm.getSchematicDirectory().string();

        output.success(msg);
    });
}

} // namespace levishematic::command
