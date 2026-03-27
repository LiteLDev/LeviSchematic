#pragma once

#include "levishematic/app/AppKernel.h"

#include "ll/api/command/CommandHandle.h"

#include "mc/server/commands/CommandBlockName.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPositionFloat.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

class Command;
class CommandOrigin;
class RenderChunkCoordinator;

namespace levishematic::command {

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

std::shared_ptr<RenderChunkCoordinator> getCoordinator(const CommandOrigin& origin);
ll::command::CommandHandle&             getOrCreateSchemCommand(bool isClient);
void                                    replyLoadFailure(
                                       CommandOutput&                        output,
                                       const std::string&                    requestedName,
                                       const placement::LoadPlacementResult& result
                                   );
void                                    logPlacementCommandFailure(
                                       std::string_view             operation,
                                       const std::filesystem::path& file,
                                       std::optional<placement::PlacementId> placementId,
                                       std::string_view             detail
                                   );

template <typename Fn>
void withSelectedPlacement(
    editor::EditorController& controller,
    CommandOutput&            output,
    Fn&&                      fn,
    const char*               errorMessage = "No placement selected."
) {
    auto const* selected = controller.selectedPlacement();
    if (!selected) {
        output.error(errorMessage);
        return;
    }

    fn(*selected);
}

template <typename ReplyFn>
void flushPlacementRefreshAndReply(
    const CommandOrigin& origin,
    CommandOutput&       output,
    ReplyFn&&            reply
) {
    if (app::hasAppKernel()) {
        app::getAppKernel().controller().flushProjectionRefresh(getCoordinator(origin));
    }
    reply(output);
}

void registerSchemLoadCommands(ll::command::CommandHandle& schemCmd);
void registerSchemPlacementCommands(ll::command::CommandHandle& schemCmd);
void registerSchemSelectionCommands(ll::command::CommandHandle& schemCmd);
void registerSchemTransformCommands(ll::command::CommandHandle& schemCmd);

} // namespace levishematic::command
