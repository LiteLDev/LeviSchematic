#include "Command.h"

#include "levishematic/LeviShematic.h"
#include "levishematic/command/CommandShared.h"

namespace levishematic::command {
namespace {

auto& getLogger() {
    return levishematic::LeviShematic::getInstance().getSelf().getLogger();
}

} // namespace

void registerCommands(bool isClient) {
    registerRenderTestCommands(isClient);

    auto& schemCmd = getOrCreateSchemCommand(isClient);
    registerSchemLoadCommands(schemCmd);
    registerSchemTransformCommands(schemCmd);
    registerSchemPlacementCommands(schemCmd);
    registerSchemSelectionCommands(schemCmd);

    getLogger().info("LeviSchematic commands registered");
}

} // namespace levishematic::command
