#include "levishematic/command/CommandShared.h"

#include "levishematic/core/DataManager.h"
#include "levishematic/render/ProjectionRenderer.h"

#include "ll/api/command/CommandRegistrar.h"

#include "mc/deps/core/math/Color.h"
#include "mc/server/commands/Command.h"
#include "mc/server/commands/CommandBlockNameResult.h"
#include "mc/world/level/BlockPos.h"

#include <vector>

namespace levishematic::command {
namespace {

std::vector<render::ProjEntry> sTestEntries;

} // namespace

void registerRenderTestCommands(bool isClient) {
    auto& dm = core::DataManager::getInstance();

    auto& renderCmd =
        ll::command::CommandRegistrar::getInstance(isClient)
            .getOrCreateCommand("rendert", "Projection render test");

    renderCmd.overload<RenderTestParam>()
        .required("pos")
        .required("blockName")
        .required("title_date")
        .required("mode_")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     RenderTestParam const& param,
                     Command const&      cmd) {
            auto& projection = dm.getProjectionState();
            auto  blockPos   = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
            auto  coordinator = getCoordinator(origin);

            if (param.mode_ == RenderTestParam::mode::single) {
                projection.replaceEntriesAndTriggerRebuild(
                    {render::ProjEntry{
                        blockPos,
                        param.blockName.resolveBlock(param.title_date).mBlock,
                        mce::Color(0.75f, 0.85f, 1.0f, 0.85f)
                    }},
                    coordinator
                );
                sTestEntries.clear();
            } else {
                sTestEntries.push_back({
                    blockPos,
                    param.blockName.resolveBlock(param.title_date).mBlock,
                    mce::Color(0.75f, 0.85f, 1.0f, 0.85f)
                });
                projection.replaceEntriesAndTriggerRebuild(sTestEntries, coordinator);
            }

            output.success("Projection updated");
        });

    renderCmd.overload().execute([&](CommandOrigin const& origin, CommandOutput& output) {
        dm.getProjectionState().clearAndTriggerRebuild(getCoordinator(origin));
        sTestEntries.clear();
        output.success("Projection cleared");
    });
}

void clearRenderTestEntries() {
    sTestEntries.clear();
}

} // namespace levishematic::command
