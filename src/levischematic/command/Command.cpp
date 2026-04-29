#include "Command.h"

#include "levischematic/LeviSchematic.h"
#include "levischematic/command/CommandShared.h"

namespace levischematic::command {
namespace {

auto& getLogger() {
    return levischematic::LeviSchematic::getInstance().getSelf().getLogger();
}

} // namespace

void registerCommands(bool isClient) {
    auto& schemCmd = getOrCreateSchemCommand(isClient);
    registerSchemLoadCommands(schemCmd);
    registerSchemTransformCommands(schemCmd);
    registerSchemPlacementCommands(schemCmd);
    registerSchemSelectionCommands(schemCmd);
    registerSchemViewCommands(schemCmd);

    getLogger().info("LeviSchematic commands registered");
}

} // namespace levischematic::command
