#include "RuntimeState.h"
#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/app/lifecycle/AppKernel.h"
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
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/threading/TaskGroup.h"
#include "mc/resources/ResourceLoadManager.h"
#include <stdexcept>
namespace levischematic::app {
void start() {
    auto lifecycle = gSessionGate.control();
    if (!gAppKernel || gTickListener || gStopRequested || gLeaveRequested) return;
    input::startInputEvents();
    auto& bus = ll::event::EventBus::getInstance();
    gJoinListener =
        bus.emplaceListener<ll::event::client::ClientJoinLevelEvent>([](auto& event) { clientReady(event.self()); });
    gTickListener          = bus.emplaceListener<ll::event::world::ClientLevelTickEvent>([](auto&) {
        auto c = ll::service::getClientInstance();
        if (c) clientTick(*c);
    });
    gSaveTickListener      = bus.emplaceListener<ll::event::world::ServerLevelTickEvent>([](auto& event) {
        auto session = tryAccessSession();
        if (!session) return;
        util::WorkBudget budget(configStore().snapshot().performance.tickBudgetMs);
        session->saves().tickServer(event.level(), session->sessionGeneration(), budget);
        session->worldEdit().tickServer(event.level(), session->sessionGeneration(), budget);
    });
    gClientCommandListener = bus.emplaceListener<ll::event::command::ClientCommandRegisterEvent>([](auto&) {
        if (auto session = accessSession(); session && session->usesClientCommands()) {
            command::registerCommands(true);
        }
    });
    gServerCommandListener = bus.emplaceListener<ll::event::command::ServerCommandRegisterEvent>([](auto&) {
        if (auto session = accessSession(); session && !session->usesClientCommands()) {
            command::registerCommands(false);
        }
    });
    hook::registerTickHooks();
    if (auto c = ll::service::getClientInstance()) {
        if (!gReloading && !resourceLoadingComplete(*c)) {
            gReloading    = true;
            gReloadQueued = true;
        }
        gAppKernel->requestSession(*c);
    }
}

} // namespace levischematic::app
