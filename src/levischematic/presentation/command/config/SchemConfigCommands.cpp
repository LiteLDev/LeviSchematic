#include "levischematic/presentation/command/shared/CommandShared.h"
#include "levischematic/workspace/config/ConfigStore.h"
namespace levischematic::command {
void registerSchemConfigCommands(ll::command::CommandHandle& schemCmd) {
    schemCmd.overload().text("config").text("reload").execute([](CommandOrigin const&, CommandOutput& output) {
        auto error = app::configStore().reload();
        if (error.empty()) output.success("Configuration queued for the next client tick.");
        else output.error(error);
    });
    schemCmd.overload().text("config").text("save").execute([](CommandOrigin const&, CommandOutput& output) {
        auto error = app::configStore().save();
        if (error.empty()) output.success("Configuration saved.");
        else output.error(error);
    });
}
}
