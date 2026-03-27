#include "levishematic/command/CommandShared.h"

#include "levishematic/LeviShematic.h"

#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/world/level/dimension/Dimension.h"

namespace levishematic::command {
namespace {

auto& getLogger() {
    return levishematic::LeviShematic::getInstance().getSelf().getLogger();
}

} // namespace

std::shared_ptr<RenderChunkCoordinator> getCoordinator(const CommandOrigin& origin) {
    auto client = ll::service::getClientInstance();
    if (!client || !client->getLevelRenderer()) {
        return nullptr;
    }

    auto* dimension = origin.getDimension();
    if (!dimension) {
        return nullptr;
    }

    auto dimId = dimension->getDimensionId();
    return client->getLevelRenderer()->mRenderChunkCoordinators->at(dimId);
}

ll::command::CommandHandle& getOrCreateSchemCommand(bool isClient) {
    return ll::command::CommandRegistrar::getInstance(isClient)
        .getOrCreateCommand("schem", "LeviSchematic commands");
}

void replyLoadFailure(
    CommandOutput&                        output,
    const std::string&                    requestedName,
    const placement::LoadPlacementResult& result
) {
    if (result.error) {
        auto target = result.resolvedPath.empty() ? requestedName : result.resolvedPath.string();
        logPlacementCommandFailure("command.loadSchematic", result.resolvedPath, 0, result.error->describe(target));
        output.error(result.error->describe(target));
        return;
    }

    logPlacementCommandFailure("command.loadSchematic", requestedName, 0, "unknown load failure");
    output.error("Failed to load schematic: {}", requestedName);
}

void logPlacementCommandFailure(
    std::string_view                 operation,
    const std::filesystem::path&     file,
    placement::SchematicPlacement::Id placementId,
    std::string_view                 detail
) {
    getLogger().warn(
        "Placement operation failed [operation={}, file={}, placementId={}]: {}",
        operation,
        file.string(),
        placementId,
        detail
    );
}

} // namespace levishematic::command
