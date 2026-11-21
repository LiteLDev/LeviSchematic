#include "levischematic/presentation/command/shared/CommandShared.h"
#include "levischematic/schematic/io/SchematicPathResolver.h"
#include "levischematic/shared/io/Utf8Path.h"
#include "levischematic/workspace/config/ConfigStore.h"
#include "mc/server/commands/Command.h"

namespace levischematic::command {
struct LoadTaskParam {
    int taskId;
};
struct SchemConvertParam {
    std::string source;
    std::string target;
};
namespace {
void enqueueConvert(CommandOutput& output, SchemConvertParam const& param, bool overwrite) {
    auto session = acquireSession(output);
    if (!session) return;
    if (!session->runtimeReady()) {
        output.error("Schematic directory is unavailable.");
        return;
    }
    auto source = session->runtime().resolveSchematicPath(param.source);
    auto target = schematic::SchematicPathResolver(session->runtime().schematicDirectory())
                      .makeFilePath(util::pathFromUtf8(param.target));
    auto result = session->conversions().request(source, target, overwrite);
    if (!result) {
        output.error(result.error());
        return;
    }
    output.success(
        "Converting '{}' -> '{}' [task: {}]. Use /schem convert-status.",
        source.filename().string(),
        target.filename().string(),
        result.value()
    );
}
void enqueueLoad(CommandOutput& output, std::string const& filename, BlockPos pos, int dimension) {
    auto session = acquireSession(output);
    if (!session) return;
    if (!session->runtimeReady()) {
        output.error("Schematic directory is unavailable.");
        return;
    }
    auto result = session->loads().request(
        session->runtime().resolveSchematicPath(filename),
        pos,
        dimension,
        session->sessionGeneration(),
        app::configStore().snapshot().performance.maxLoadRequests
    );
    if (!result) {
        output.error(result.error());
        return;
    }
    output.success("Schematic load queued [task: {}]. Use /schem load status <taskId>.", result.value());
}
}
void registerSchemLoadCommands(ll::command::CommandHandle& schemCmd) {
    schemCmd.overload<SchemLoadParam>()
        .text("load")
        .required("filename")
        .required("pos")
        .execute(
            [](CommandOrigin const& origin, CommandOutput& output, SchemLoadParam const& param, Command const& cmd) {
                auto* dimension = origin.getDimension();
                enqueueLoad(
                    output,
                    param.filename,
                    BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos)),
                    dimension ? static_cast<int>(dimension->getDimensionId()) : 0
                );
            }
        );
    schemCmd.overload<SchemNamedParam>()
        .text("load")
        .required("filename")
        .execute([](CommandOrigin const& origin, CommandOutput& output, SchemNamedParam const& param) {
            auto* dimension = origin.getDimension();
            enqueueLoad(
                output,
                param.filename,
                BlockPos(origin.getWorldPosition()),
                dimension ? static_cast<int>(dimension->getDimensionId()) : 0
            );
        });
    schemCmd.overload<LoadTaskParam>().text("load").text("status").required("taskId").execute(
        [](CommandOrigin const&, CommandOutput& output, LoadTaskParam const& param) {
            auto session = acquireSession(output);
            if (!session) return;
            auto status = session->loads().status(param.taskId, session->sessionGeneration());
            if (!status) {
                output.error("Load task not found in this session.");
                return;
            }
            output.success(
                "Load #{}: {} ({}/{}) {}",
                status->id,
                status->phase,
                status->completed,
                status->total,
                status->detail
            );
        }
    );
    schemCmd.overload<LoadTaskParam>().text("load").text("cancel").required("taskId").execute(
        [](CommandOrigin const&, CommandOutput& output, LoadTaskParam const& param) {
            auto session = acquireSession(output);
            if (!session) return;
            if (session->loads().cancel(param.taskId, session->sessionGeneration())) output.success("Load cancelled.");
            else output.error("Load task is unknown or already finished.");
        }
    );
    schemCmd.overload<SchemConvertParam>().text("convert").required("source").required("target").execute(
        [](CommandOrigin const&, CommandOutput& output, SchemConvertParam const& param) {
            enqueueConvert(output, param, false);
        }
    );
    schemCmd.overload<SchemConvertParam>()
        .text("convert-overwrite")
        .required("source")
        .required("target")
        .execute([](CommandOrigin const&, CommandOutput& output, SchemConvertParam const& param) {
            enqueueConvert(output, param, true);
        });
    schemCmd.overload().text("convert-status").execute([](CommandOrigin const&, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto status = session->conversions().status();
        if (!status) {
            output.success("No conversion has run in this game session.");
            return;
        }
        auto text = "Convert #" + std::to_string(status->id) + ": " + status->source.filename().string() + " -> "
                  + status->target.filename().string() + " [" + status->phase + "] " + status->detail;
        if (status->phase == "failed") output.error(text);
        else output.success(text);
    });
    schemCmd.overload().text("convert-cancel").execute([](CommandOrigin const&, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        if (session->conversions().cancel()) output.success("Conversion cancellation requested.");
        else output.error("No conversion is running.");
    });
    schemCmd.overload().text("files").execute([&](CommandOrigin const&, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        if (!session->runtimeReady()) {
            output.error("Schematic directory is unavailable for this world; retry after the world path is ready.");
            return;
        }
        auto const& runtime = session->runtime();
        auto        files   = runtime.listAvailableSchematics();
        if (files.empty()) {
            output.success(
                "No schematic files found in: {}\nPlace .mcstructure, .litematic or .schemset.json files there "
                "and try again.",
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

}
