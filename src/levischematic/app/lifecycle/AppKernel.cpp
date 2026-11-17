#include "levischematic/app/lifecycle/AppKernel.h"
#include "RuntimeState.h"
#include "WorldSession.h"
#include "levischematic/app/bootstrap/LeviSchematic.h"
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
namespace {
template <typename T>
T& req(T* p, char const* n) {
    if (!p) throw std::logic_error(std::string("session unavailable: ") + n);
    return *p;
}
}
AppKernel::AppKernel() : mGate(gSessionGate) {}
AppKernel::~AppKernel() = default;
render::ActorProjection& AppKernel::actors() { return req(mSession.get(), "actors").actors; }
bool                     AppKernel::hasSession() const noexcept { return mSession && mState == State::Active; }
uint64_t                 AppKernel::resourceGeneration() const noexcept { return mResourceGeneration.load(); }
bool AppKernel::runtimeReady() const noexcept { return hasSession() && mSession->runtime.hasSchematicDirectory(); }
bool AppKernel::hudReady() const noexcept { return hasSession() && !gReloading; }
bool AppKernel::usesClientCommands() const noexcept { return mUsesClientCommands; }
RuntimeContext&                 AppKernel::runtime() { return req(mSession.get(), "runtime").runtime; }
PlacementService&               AppKernel::placement() { return *req(mSession.get(), "placement").placementService; }
SelectionService&               AppKernel::selection() { return *req(mSession.get(), "selection").selectionService; }
ViewService&                    AppKernel::view() { return *req(mSession.get(), "view").viewService; }
render::SchematicChunkRenderer& AppKernel::schematicRenderer() { return *req(mSession.get(), "renderer").renderer; }
verifier::VerifierService&      AppKernel::verifier() { return *req(mSession.get(), "verifier").verifierService; }
input::InputHandler&            AppKernel::input() { return req(mSession.get(), "input").input; }
ToolService&                    AppKernel::tools() { return *req(mSession.get(), "tools").tools; }
SurvivalService&                AppKernel::survival() { return *req(mSession.get(), "survival").survival; }
WorldEditService&               AppKernel::worldEdit() { return *req(mSession.get(), "worldEdit").worldEdit; }
ProjectService&                 AppKernel::projects() { return req(mSession.get(), "projects").projects; }
SignService&                    AppKernel::signs() { return req(mSession.get(), "signs").signs; }
SessionPersistence&             AppKernel::sessions() { return *req(mSession.get(), "sessions").persistence; }
ProjectServices                 AppKernel::projectServices() {
    return {selection(), saves(), loads(), placement(), worldEdit(), sessionGeneration()};
}
void AppKernel::startSession(ClientInstance& client) {
    if (mSession || mState == State::Stopping || !client.isPrimaryClient() || !client.getLevel()
        || !client.getLocalPlayer() || !mReadyPending)
        return;
    bool const useClientCommands = !static_cast<bool>(ll::service::getServerInstance());
    mState                       = State::Starting;
    try {
        auto s = std::make_unique<WorldSession>();
        s->configureSchematicDirectory();
        s->renderer->applyConfig(configStore().snapshot());
        s->input.initialize(
            {.viewService            = s->viewService.get(),
             .refreshProjectionState = [this]() { refreshCurrentClientProjectionState(*this); },
             .toolService            = s->tools.get()}
        );
        mUsesClientCommands = useClientCommands;
        if (ll::service::getCommandRegistry(mUsesClientCommands)) {
            command::registerCommands(mUsesClientCommands);
            s->commandsRegistered = true;
        }
        hook::registerRuntimeHooks();
        s->verifierService->handleJoinLevel(client);
        if (!gReloading && client.isReadyToRender()) s->renderer->resume();
        mSession      = std::move(s);
        mState        = State::Active;
        mReadyPending = false;
        mResourceGeneration.fetch_add(1);
        mGate.publish();
    } catch (...) {
        mGate.close();
        LeviSchematic::getInstance().getSelf().getLogger().error(
            "LeviSchematic session initialization failed; rolled back to empty state"
        );
        hook::unregisterRuntimeHooks();
        mSession.reset();
        mUsesClientCommands = false;
        mReadyPending       = false;
        mState              = State::Empty;
    }
}
void AppKernel::requestSession(ClientInstance& client) {
    mReadyPending = true;
    startSession(client);
}
void AppKernel::endSession() {
    mGate.close();
    mReadyPending = false;
    if (!mSession) return;
    mSession->input.shutdown();
    mSession->worldEdit->cancel("Session ending; submitted world changes retained.");
    mLoads.cancelAll();
    mSaves.cancel();
    mSaveNotified = mSaves.progress().id;
    mSession->persistence->cancelRestore();
    mSession->persistence->flush();
    mState = State::Stopping;
    hook::unregisterRuntimeHooks();
    mSession.reset();
    mUsesClientCommands = false;
    mState              = State::Empty;
    mResourceGeneration.fetch_add(1);
}
void AppKernel::invalidateRuntime(std::optional<int>) {
    mLoads.cancelAll();
    mSaves.cancel();
    if (!hasSession()) return;
    mSession->persistence->cancelRestore();
    input::resetInputState();
    mSession->survival->reset();
    mSession->worldEdit->cancel("Runtime invalidated; world changes retained.");
    mSession->projects.cancel(projectServices());
    mSession->signs.cancel();
    mSession->renderer->suspend();
    mSession->actors.clear();
    mSession->verifierService->detachFromRuntime();
    mSession->verifierService->clear();
    mResourceGeneration.fetch_add(1);
}
void AppKernel::resetRenderer() {
    if (hasSession()) {
        mSession->renderer->resetMaterialResources();
        mSession->actors.clear();
        mResourceGeneration.fetch_add(1);
    }
}
void AppKernel::invalidateRenderResources() {
    if (!hasSession()) return;
    mSession->survival->reset();
    mSession->worldEdit->cancel("Resources reloading; prepare a new world operation.");
    mSession->renderer->suspend(true);
    mSession->actors.clear();
    mResourceGeneration.fetch_add(1);
}

}
