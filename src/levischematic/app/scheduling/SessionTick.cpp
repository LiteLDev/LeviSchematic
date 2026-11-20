#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/app/lifecycle/AppKernel.h"
#include "levischematic/app/lifecycle/RuntimeState.h"
#include "levischematic/app/lifecycle/WorldSession.h"
#include "levischematic/app/lifecycle/hooks/RuntimeHooks.h"
#include "levischematic/presentation/command/registration/Command.h"
#include "levischematic/presentation/ui/model/Ui.h"
#include "levischematic/render/actor/ActorProjection.h"
#include "levischematic/render/chunk/SchematicChunkRenderer.h"
#include "levischematic/schematic/io/SchematicLoader.h"
#include "levischematic/schematic/placement/store/PlacementStore.h"
#include "levischematic/schematic/placement/view/ProjectionRefresh.h"
#include "levischematic/selection/capture/SelectionExporter.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/event/command/ClientCommandRegisterEvent.h"
#include "ll/api/event/command/ServerCommandRegisterEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/memory/Memory.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/service/ServerInfo.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/threading/TaskGroup.h"
#include "mc/resources/ResourceLoadManager.h"
#include <stdexcept>
namespace levischematic::app {
bool resourceLoadingComplete(ClientInstance& client) {
    auto manager = client.getMinecraftGame_DEPRECATED().getResourceLoadManager();
    if (!manager->mDispatchTaskGroup.get()->isEmpty()) return false;
    for (auto const& [type, group] : manager->mResourceLoadTaskGroups.get()) {
        auto const& tasks = group->mUnk6c2afd.as<std::unique_ptr<TaskGroup>>();
        if (!tasks->isEmpty()) return false;
    }
    return true;
}
void AppKernel::maintain(ClientInstance& client) {
    if (auto config = configStore().applyPending(); config && hasSession()) mSession->renderer->applyConfig(*config);
    if (!hasSession()) {
        if (mReadyPending) startSession(client);
        return;
    }
    mSession->input.pump(client, sessionGeneration());
    if (!mSession->commandsRegistered && ll::service::getCommandRegistry(mUsesClientCommands)) {
        command::registerCommands(mUsesClientCommands);
        mSession->commandsRegistered = true;
    }
    mSession->configureSchematicDirectory();
    if (gReloading && gReloadQueued && resourceLoadingComplete(client)) {
        gReloading    = false;
        gReloadQueued = false;
    }
    if (!gReloading && client.getLocalPlayer() && client.getLevel() && client.isReadyToRender()) {
        mSession->verifierService->ensureRuntimeBindings(client);
        mSession->renderer->resume();
        auto             config = configStore().snapshot();
        util::WorkBudget budget(config.performance.tickBudgetMs);
        util::WorkBudget changesBudget(config.performance.tickBudgetMs * 0.5);
        mSession->verifierService->tickChanges(changesBudget);
        mSession->verifierService->tick(changesBudget);
        auto& level = *client.getLevel();
        mSession->placementService->resources().tick(level, budget);
        mSession->selectionService->tickGrow(client.getLocalPlayer()->getDimension(), budget);
        mSession->actors.tick(client, *mSession->placementService, budget);
        mSession->worldEdit->tick(client, sessionGeneration(), budget);
        auto worldProgress = mSession->worldEdit->progress();
        if (worldProgress.id
            && (worldProgress.id != mSession->worldNotifiedId || worldProgress.phase != mSession->worldNotifiedPhase)) {
            mSession->worldNotifiedId    = worldProgress.id;
            mSession->worldNotifiedPhase = worldProgress.phase;
            auto text = std::string("World task #") + std::to_string(worldProgress.id) + " "
                      + worldedit::phaseName(worldProgress.phase)
                      + " observed=" + std::to_string(worldProgress.observed) + "/"
                      + std::to_string(worldProgress.total) + " verified=" + std::to_string(worldProgress.verified)
                      + " skipped=" + std::to_string(worldProgress.skipped)
                      + " rejected=" + std::to_string(worldProgress.rejected) + " " + worldProgress.detail;
            LeviSchematic::getInstance().getSelf().getLogger().info(text);
            if (worldProgress.terminal() || worldProgress.phase == worldedit::Phase::Ready)
                client.getLocalPlayer()->displayClientMessage(text, std::nullopt);
        }
        if (mSession->worldEdit->ownsInteraction()) mSession->survival->reset();
        else mSession->survival->tick(client, budget);
        mSession->persistence->tick(client, budget);
        mSession->viewService->follow(BlockPos(client.getLocalPlayer()->getPosition()));
        mSaves.tick(client, sessionGeneration(), budget);
        mSession->projects.tick(projectServices(), client, budget);
        auto save = mSaves.progress();
        if (save.id > mSaveNotified && save.terminal) {
            mSaveNotified = save.id;
            auto message  = save.phase + ": " + save.path.string() + " " + save.detail;
            client.getLocalPlayer()->displayClientMessage(message, std::nullopt);
            LeviSchematic::getInstance().getSelf().getLogger().info(message);
        }
        mSession->loadFirst = !mSession->loadFirst;
        auto notices        = mLoads.tick(sessionGeneration(), *mSession->placementService, level, budget);
        for (auto const& message : notices) {
            client.getLocalPlayer()->displayClientMessage(message, std::nullopt);
            LeviSchematic::getInstance().getSelf().getLogger().info(message);
        }
    }
}

}
