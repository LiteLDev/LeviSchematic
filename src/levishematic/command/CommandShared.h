#pragma once

#include "levishematic/core/DataManager.h"

#include "ll/api/command/CommandHandle.h"

#include "mc/server/commands/CommandBlockName.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPositionFloat.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

class Command;
class CommandOrigin;
class RenderChunkCoordinator;

namespace levishematic::command {

struct RenderTestParam {
    CommandBlockName     blockName;
    CommandPositionFloat pos;
    int                  title_date;
    enum class mode { add, single } mode_;
};

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

std::shared_ptr<RenderChunkCoordinator> getCoordinator(const CommandOrigin& origin);
ll::command::CommandHandle&             getOrCreateSchemCommand(bool isClient);
void                                    replyLoadFailure(
                                       CommandOutput&                        output,
                                       const std::string&                    requestedName,
                                       const placement::LoadPlacementResult& result
                                   );
void                                    logPlacementCommandFailure(
                                       std::string_view                operation,
                                       const std::filesystem::path&    file,
                                       placement::SchematicPlacement::Id placementId,
                                       std::string_view                detail
                                   );

template <typename Fn>
void withSelectedPlacement(
    placement::PlacementManager& pm,
    CommandOutput&               output,
    Fn&&                         fn,
    const char*                  errorMessage = "No placement selected."
) {
    auto* selected = pm.getSelected();
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
    core::DataManager::getInstance().flushPlacementRefresh(getCoordinator(origin));
    reply(output);
}

void registerRenderTestCommands(bool isClient);
void clearRenderTestEntries();
void registerSchemLoadCommands(ll::command::CommandHandle& schemCmd);
void registerSchemPlacementCommands(ll::command::CommandHandle& schemCmd);
void registerSchemSelectionCommands(ll::command::CommandHandle& schemCmd);
void registerSchemTransformCommands(ll::command::CommandHandle& schemCmd);

} // namespace levishematic::command
