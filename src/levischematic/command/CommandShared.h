#pragma once

#include "levischematic/app/AppKernel.h"

#include "ll/api/command/CommandHandle.h"

#include "mc/server/commands/CommandBlockName.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPositionFloat.h"
#include "mc/world/level/dimension/Dimension.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

class Command;
class CommandOrigin;
class RenderChunkCoordinator;

namespace levischematic::command {

struct SchemLoadParam {
    std::string          filename;
    CommandPositionFloat pos;
};

struct SchemNamedParam {
    std::string filename;
};

struct SchemMoveParam {
    int dx;
    int dy;
    int dz;
};

struct SchemRotateParam {
    enum class direction { cw90, ccw90, r180 } dir;
};

struct SchemMirrorParam {
    enum class axis { x, z, none } axis_;
};

struct SchemRemoveParam {
    int id;
};

struct SchemSelectParam {
    int id;
};

struct SchemOriginParam {
    CommandPositionFloat pos;
};

struct SchemBlockPosParam {
    CommandPositionFloat pos;
};

struct SchemBlockSetParam {
    CommandPositionFloat pos;
    CommandBlockName     blockName;
    int                  title_date;
};

struct SchemBlockSetSimpleParam {
    CommandPositionFloat pos;
    CommandBlockName     blockName;
};

struct SchemViewYValueParam {
    int value;
};

struct SchemViewYRangeParam {
    int minY;
    int maxY;
};

std::shared_ptr<RenderChunkCoordinator> getCoordinator(const CommandOrigin& origin);
ll::command::CommandHandle&             getOrCreateSchemCommand(bool isClient);
void                                    refreshProjectionStateForOrigin(const CommandOrigin& origin);
void                                    logPlacementCommandFailure(
                                       std::string_view                      operation,
                                       const std::filesystem::path&          file,
                                       std::optional<placement::PlacementId> placementId,
                                       std::string_view                      detail
                                   );
void                                    replyPlacementError(
                                       CommandOutput&             output,
                                       std::string_view           operation,
                                       app::PlacementError const& error
                                   );
void                                    replySelectionError(
                                       CommandOutput&             output,
                                       std::string_view           operation,
                                       std::string_view           target,
                                       app::SelectionError const& error
                                   );

template <typename Fn>
void withSelectedPlacement(
    app::PlacementService& placementService,
    CommandOutput&         output,
    Fn&&                   fn,
    const char*            fallbackErrorMessage = nullptr
) {
    auto selected = placementService.requireSelectedPlacement();
    if (!selected) {
        if (fallbackErrorMessage && selected.error().code == app::PlacementError::Code::NoSelection) {
            output.error(fallbackErrorMessage);
            return;
        }
        replyPlacementError(output, "command.withSelectedPlacement", selected.error());
        return;
    }

    fn(*selected.value());
}

template <typename ReplyFn>
void flushPlacementRefreshAndReply(
    const CommandOrigin& origin,
    CommandOutput&       output,
    ReplyFn&&            reply
) {
    refreshProjectionStateForOrigin(origin);
    reply(output);
}

void registerSchemLoadCommands(ll::command::CommandHandle& schemCmd);
void registerSchemPlacementCommands(ll::command::CommandHandle& schemCmd);
void registerSchemSelectionCommands(ll::command::CommandHandle& schemCmd);
void registerSchemTransformCommands(ll::command::CommandHandle& schemCmd);
void registerSchemViewCommands(ll::command::CommandHandle& schemCmd);

} // namespace levischematic::command
