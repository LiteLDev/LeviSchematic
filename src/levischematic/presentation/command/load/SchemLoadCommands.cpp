#include "levischematic/presentation/command/shared/CommandShared.h"
#include "levischematic/schematic/io/SchematicPathResolver.h"
#include "levischematic/shared/i18n/I18n.h"
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
        output.error(tr("cmd.load.directory_unavailable"));
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
    output.success(tr("cmd.load.converting", source.filename().string(), target.filename().string(), result.value()));
}
void enqueueLoad(CommandOutput& output, std::string const& filename, BlockPos pos, int dimension) {
    auto session = acquireSession(output);
    if (!session) return;
    if (!session->runtimeReady()) {
        output.error(tr("cmd.load.directory_unavailable"));
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
    output.success(tr("cmd.load.queued", result.value()));
}
} // namespace
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
                output.error(tr("cmd.load.task_not_found"));
                return;
            }
            output.success(
                tr("cmd.load.status", status->id, status->phase, status->completed, status->total, status->detail)
            );
        }
    );
    schemCmd.overload<LoadTaskParam>().text("load").text("cancel").required("taskId").execute(
        [](CommandOrigin const&, CommandOutput& output, LoadTaskParam const& param) {
            auto session = acquireSession(output);
            if (!session) return;
            if (session->loads().cancel(param.taskId, session->sessionGeneration()))
                output.success(tr("cmd.load.cancelled"));
            else output.error(tr("cmd.load.cancel_unknown"));
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
            output.success(tr("cmd.load.no_conversion"));
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
        if (session->conversions().cancel()) output.success(tr("cmd.load.conversion_cancel_requested"));
        else output.error(tr("cmd.load.conversion_not_running"));
    });
    schemCmd.overload().text("files").execute([&](CommandOrigin const&, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        if (!session->runtimeReady()) {
            output.error(tr("cmd.load.directory_unavailable_world"));
            return;
        }
        auto const& runtime = session->runtime();
        auto        files   = runtime.listAvailableSchematics();
        if (files.empty()) {
            output.success(tr("cmd.load.no_files", runtime.schematicDirectory().string()));
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
