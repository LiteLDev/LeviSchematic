#include "levischematic/command/CommandShared.h"

#include "levischematic/LeviSchematic.h"
#include "levischematic/app/ProjectionRefresh.h"

#include "ll/api/command/CommandRegistrar.h"
#include "mc/world/level/dimension/Dimension.h"

namespace levischematic::command {
namespace {


auto& getLogger() { return levischematic::LeviSchematic::getInstance().getSelf().getLogger(); }

} 

ll::command::CommandHandle& getOrCreateSchemCommand(bool isClient) {
    return ll::command::CommandRegistrar::getInstance(isClient).getOrCreateCommand("schem", "LeviSchematic commands");
}

void refreshProjectionStateForOrigin(app::AppKernel& session, CommandOrigin const& origin) {
    app::refreshProjectionStateForDimension(session, origin.getDimension());
}

void logPlacementCommandFailure(
    std::string_view                      operation,
    std::filesystem::path const&          file,
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

void replyPlacementError(CommandOutput& output, std::string_view operation, app::PlacementError const& error) {
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

} 
