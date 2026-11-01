#include "Command.h"

#include "levischematic/LeviSchematic.h"
#include "levischematic/command/CommandShared.h"
#include "levischematic/ui/Ui.h"

namespace levischematic::command {
namespace {


auto& getLogger() { return levischematic::LeviSchematic::getInstance().getSelf().getLogger(); }

} 

void registerCommands(bool isClient) {
    auto& schemCmd = getOrCreateSchemCommand(isClient);
    schemCmd.overload().text("ui").text("status").execute([](CommandOrigin const&, CommandOutput& output) {
        output.success(ui::diagnostics());
    });
    schemCmd.overload().text("ui").execute([](CommandOrigin const&, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        ui::requestOpen();
        output.success("Requested projection UI.");
    });
    registerSchemLoadCommands(schemCmd);
    registerSchemTransformCommands(schemCmd);
    registerSchemPlacementCommands(schemCmd);
    registerSchemSelectionCommands(schemCmd);
    registerSchemViewCommands(schemCmd);
    registerSchemConfigCommands(schemCmd);
    registerSchemRendererCommands(schemCmd);

    getLogger().info("LeviSchematic commands registered");
}

} 
