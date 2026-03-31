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

void logPlacementCommandFailure(
    std::string_view                      operation,
    const std::filesystem::path&          file,
    std::optional<placement::PlacementId> placementId,
    std::string_view                      detail
) {
    getLogger().warn(
        "Placement operation failed [operation={}, file={}, placementId={}]: {}",
        operation,
        file.string(),
        placementId.value_or(0),
        detail
    );
}

void replyPlacementError(
    CommandOutput&             output,
    std::string_view           operation,
    app::PlacementError const& error
) {
    logPlacementCommandFailure(operation, error.path, error.placementId, error.describe());
    output.error(error.describe());
}

void replySelectionError(
    CommandOutput&             output,
    std::string_view           operation,
    std::string_view           target,
    app::SelectionError const& error
) {
    logPlacementCommandFailure(operation, error.path, std::nullopt, error.describe(target));
    output.error(error.describe(target));
}

} // namespace levishematic::command
