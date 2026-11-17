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
SessionGate                gSessionGate(drainSession);
bool                       gStopRequested    = false;
bool                       gLeaveRequested   = false;
bool                       gDraining         = false;
bool                       gReloading        = false;
bool                       gReloadQueued     = false;
unsigned                   gReloadOperations = 0;
std::unique_ptr<AppKernel> gAppKernel;
ll::event::ListenerPtr     gJoinListener;
ll::event::ListenerPtr     gTickListener;
ll::event::ListenerPtr     gSaveTickListener;
ll::event::ListenerPtr     gClientCommandListener;
ll::event::ListenerPtr     gServerCommandListener;
SessionAccess::SessionAccess(bool tryOnly)
: mLock(gSessionGate.acquire(tryOnly)),
  mKernel((mLock.owns_lock() && gAppKernel && gAppKernel->hasSession()) ? gAppKernel.get() : nullptr) {}
SessionAccess accessSession() { return SessionAccess{}; }
SessionAccess tryAccessSession() { return SessionAccess{true}; }
bool          hasAppKernel() {
    auto lock = gSessionGate.control();
    return (bool)gAppKernel;
}
bool hasActiveSession() {
    auto lock = gSessionGate.acquire();
    return lock.owns_lock() && gAppKernel && gAppKernel->hasSession();
}
void load() {
    auto lifecycle = gSessionGate.control();
    if (gAppKernel) return;
    auto error = configStore().initialize(LeviSchematic::getInstance().getSelf().getConfigDir() / "config.json");
    if (!error.empty()) LeviSchematic::getInstance().getSelf().getLogger().warn("Configuration: {}", error);
    input::registerInputDefinitions();
    gAppKernel = std::make_unique<AppKernel>();
}
void drainSession() noexcept {
    if (gDraining || (!gStopRequested && !gLeaveRequested)) return;
    gDraining = true;
    if (gStopRequested) {
        if (gAppKernel) {
            gAppKernel->endSession();
            hook::unregisterTickHooks();
            input::stopInputEvents();
            auto& b = ll::event::EventBus::getInstance();
            if (gJoinListener) b.removeListener<ll::event::client::ClientJoinLevelEvent>(gJoinListener);
            if (gTickListener) b.removeListener<ll::event::world::ClientLevelTickEvent>(gTickListener);
            if (gSaveTickListener) b.removeListener<ll::event::world::ServerLevelTickEvent>(gSaveTickListener);
            if (gClientCommandListener)
                b.removeListener<ll::event::command::ClientCommandRegisterEvent>(gClientCommandListener);
            if (gServerCommandListener)
                b.removeListener<ll::event::command::ServerCommandRegisterEvent>(gServerCommandListener);
            gJoinListener.reset();
            gTickListener.reset();
            gSaveTickListener.reset();
            gClientCommandListener.reset();
            gServerCommandListener.reset();
            gAppKernel.reset();
            gReloading        = false;
            gReloadQueued     = false;
            gReloadOperations = 0;
        }
    } else if (gAppKernel) {
        gAppKernel->endSession();
    }
    gLeaveRequested = false;
    gStopRequested  = false;
    gDraining       = false;
}
void stop() {
    gSessionGate.close();
    auto lifecycle = gSessionGate.control();
    gStopRequested = true;
}
void clientReady(ClientInstance& c) {
    auto lifecycle = gSessionGate.control();
    if (gAppKernel && !gStopRequested && !gLeaveRequested) gAppKernel->requestSession(c);
}
void clientLeaving(ClientInstance& c) {
    if (!c.isPrimaryClient()) return;
    gSessionGate.close();
    auto lifecycle  = gSessionGate.control();
    gLeaveRequested = true;
}
void clientTick(ClientInstance& c) {
    if (!c.isPrimaryClient()) return;
    {
        auto lifecycle = gSessionGate.control();
        if (gAppKernel && !gStopRequested && !gLeaveRequested) gAppKernel->maintain(c);
    }
    ui::pump();
}
void invalidateRuntime(std::optional<int> d) {
    if (auto session = accessSession()) session->invalidateRuntime(d);
}
void beginResourceReload() {
    auto lifecycle = gSessionGate.control();
    ++gReloadOperations;
    gReloading    = true;
    gReloadQueued = false;
    if (gAppKernel && gAppKernel->hasSession()) gAppKernel->invalidateRenderResources();
}
void resourceReloadQueued() {
    auto lifecycle = gSessionGate.control();
    if (gReloadOperations != 0) --gReloadOperations;
    if (gReloading) gReloadQueued = gReloadOperations == 0;
}
void sourceDestroyed(verifier_block_listener::VerifierBlockListener& listener, BlockSource& source) {
    auto lifecycle = gSessionGate.control();
    if (gAppKernel && gAppKernel->hasSession() && gAppKernel->verifier().ownsListener(listener)) {
        gAppKernel->verifier().handleSourceDestroyed(source);
    }
}

}
