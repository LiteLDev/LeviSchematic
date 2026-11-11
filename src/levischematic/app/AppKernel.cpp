#include "AppKernel.h"
#include "levischematic/ui/Ui.h"
#include "levischematic/LeviSchematic.h"
#include "levischematic/app/ProjectionRefresh.h"
#include "levischematic/command/Command.h"
#include "levischematic/hook/RuntimeHooks.h"
#include "levischematic/render/SchematicChunkRenderer.h"
#include "levischematic/render/ActorProjection.h"
#include "levischematic/schematic/placement/PlacementStore.h"
#include "levischematic/schematic/placement/SchematicLoader.h"
#include "levischematic/selection/SelectionExporter.h"
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



void drainSession() noexcept;
namespace {
SessionGate                gSessionGate(drainSession);
bool                       gStopRequested    = false;
bool                       gLeaveRequested   = false;
bool                       gDraining         = false;
bool                       gReloading        = false;
bool                       gReloadQueued     = false;
unsigned                   gReloadOperations = 0;
std::unique_ptr<AppKernel> gAppKernel;






bool resourceLoadingComplete(ClientInstance& client) {
    auto manager = client.getMinecraftGame_DEPRECATED().getResourceLoadManager();
    if (!manager->mDispatchTaskGroup.get()->isEmpty()) return false;
    for (auto const& [type, group] : manager->mResourceLoadTaskGroups.get()) {
        auto const& tasks = ll::memory::dAccess<std::unique_ptr<TaskGroup>>(&group->mUnk6c2afd, 0);
        if (!tasks->isEmpty()) return false;
    }
    return true;
}





template <typename T>
T& req(T* p, char const* n) {
    if (!p) throw std::logic_error(std::string("session unavailable: ") + n);
    return *p;
}
} 
namespace {
ll::event::ListenerPtr gJoinListener;
ll::event::ListenerPtr gTickListener;
ll::event::ListenerPtr gSaveTickListener;
ll::event::ListenerPtr gClientCommandListener;
ll::event::ListenerPtr gServerCommandListener;
} 




class AppKernel::WorldSession {
public:
    RuntimeContext                                  runtime;
    std::unique_ptr<editor::EditorState>            state;
    std::unique_ptr<placement::PlacementStore>      placementStore;
    std::unique_ptr<selection::SelectionExporter>   selectionExporter;
    std::unique_ptr<render::SchematicChunkRenderer> renderer;
    std::unique_ptr<PlacementService>               placementService;
    std::unique_ptr<SelectionService>               selectionService;
    std::unique_ptr<ViewService>                    viewService;
    std::unique_ptr<verifier::VerifierService>      verifierService;
    std::unique_ptr<SessionPersistence>             persistence;
    input::InputHandler                             input;
    std::unique_ptr<ToolService>                    tools;
    std::unique_ptr<SurvivalService>                survival;
    std::unique_ptr<WorldEditService>               worldEdit;
    ProjectService                                  projects{LeviSchematic::getInstance().getSelf().getConfigDir()};
    SignService                                     signs;
    uint64_t                                        worldNotifiedId    = 0;
    worldedit::Phase                                worldNotifiedPhase = worldedit::Phase::Idle;
    render::ActorProjection actors;
    bool                                            loadFirst = false;
    bool                                            commandsRegistered     = false;
    bool                                            directoryWarningLogged = false;
    


    void                                            configureSchematicDirectory() {
        if (runtime.hasSchematicDirectory()) return;
        auto worldPath = ll::getWorldPath();
        auto directory = worldPath ? worldPath->parent_path().parent_path() / "schematics"
                                   : LeviSchematic::getInstance().getSelf().getConfigDir() / "schematics";
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        if (!ec && std::filesystem::is_directory(directory, ec) && !ec) {
            runtime.setSchematicDirectory(directory);
            directoryWarningLogged = false;
            return;
        }
        if (!directoryWarningLogged) {
            LeviSchematic::getInstance().getSelf().getLogger().warn(
                "LeviSchematic: schematic directory unavailable at {} ({})",
                directory.string(),
                ec.message()
            );
            directoryWarningLogged = true;
        }
    }
    


    WorldSession()
    : state(std::make_unique<editor::EditorState>()),
      placementStore(std::make_unique<placement::PlacementStore>(state->placements)),
      selectionExporter(std::make_unique<selection::SelectionExporter>()),
      renderer(std::make_unique<render::SchematicChunkRenderer>()),
      placementService(std::make_unique<PlacementService>(*state, runtime, *placementStore)),
      selectionService(std::make_unique<SelectionService>(*state, runtime, *selectionExporter)),
      viewService(std::make_unique<ViewService>(state->view)),
      verifierService(std::make_unique<verifier::VerifierService>(state->verifier, state->placements, state->view)),
      persistence(
          std::make_unique<SessionPersistence>(
              *state,
              *placementStore,
              LeviSchematic::getInstance().getSelf().getConfigDir()
          )
      ),
      tools(std::make_unique<ToolService>(*state, *selectionService, *placementService)),
      survival(std::make_unique<SurvivalService>(*placementService)),
      worldEdit(std::make_unique<WorldEditService>(*state, *placementService)) {}
    
    


    ~WorldSession() = default;
};
AppKernel::AppKernel() : mGate(gSessionGate) {}
AppKernel::~AppKernel() = default;
render::ActorProjection& AppKernel::actors() { return req(mSession.get(), "actors").actors; }
bool AppKernel::hasSession() const noexcept {
    return mSession && mState == State::Active;
}
uint64_t AppKernel::resourceGeneration() const noexcept { return mResourceGeneration.load(); }
bool     AppKernel::runtimeReady() const noexcept { return hasSession() && mSession->runtime.hasSchematicDirectory(); }
bool     AppKernel::hudReady() const noexcept { return hasSession() && !gReloading; }
bool     AppKernel::usesClientCommands() const noexcept { return mUsesClientCommands; }
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
void                            AppKernel::startSession(ClientInstance& client) {
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
            {.viewService = s->viewService.get(),
             .refreshProjectionState =
                 


             [this]() { refreshCurrentClientProjectionState(*this); },
             .toolService = s->tools.get()}
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
        auto config = configStore().snapshot();
        util::WorkBudget budget(config.performance.tickBudgetMs);
        mSession->placementService->resources().tick(budget);
        mSession->selectionService->tickGrow(client.getLocalPlayer()->getDimension(),budget);
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
        mSession->projects.tick(*this, client, budget);
        auto save = mSaves.progress();
        if (save.id > mSaveNotified && save.terminal) {
            mSaveNotified = save.id;
            auto message = save.phase + ": " + save.path.string() + " " + save.detail;
            client.getLocalPlayer()->displayClientMessage(message, std::nullopt);
            LeviSchematic::getInstance().getSelf().getLogger().info(message);
        }
        mSession->loadFirst = !mSession->loadFirst;
        if (!mSession->loadFirst) mSession->verifierService->tick(budget);
        auto notices = mLoads.tick(sessionGeneration(), *mSession->placementService, budget);
        for (auto const& message : notices) {
            client.getLocalPlayer()->displayClientMessage(message, std::nullopt);
            LeviSchematic::getInstance().getSelf().getLogger().info(message);
        }
        if (mSession->loadFirst) mSession->verifierService->tick(budget);
    }
}
void AppKernel::invalidateRuntime(std::optional<int>) {
    mLoads.cancelAll();
    mSaves.cancel();
    if (!hasSession()) return;
    mSession->persistence->cancelRestore();
    input::resetInputState();
    mSession->survival->reset();
    mSession->worldEdit->cancel("Runtime invalidated; world changes retained.");
    mSession->projects.cancel(*this);
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
void start() {
    auto lifecycle = gSessionGate.control();
    if (!gAppKernel || gTickListener || gStopRequested || gLeaveRequested) return;
    input::startInputEvents();
    auto& bus              = ll::event::EventBus::getInstance();
    gJoinListener =
        bus.emplaceListener<ll::event::client::ClientJoinLevelEvent>(
        



        [](auto& event) { clientReady(event.self()); });
    gTickListener          = bus.emplaceListener<ll::event::world::ClientLevelTickEvent>(
    



    [](auto&) {
        auto c = ll::service::getClientInstance();
        if (c) clientTick(*c);
    });
    
    gSaveTickListener = bus.emplaceListener<ll::event::world::ServerLevelTickEvent>([](auto& event) {
        auto session = tryAccessSession();
        if (!session) return;
        util::WorkBudget budget(configStore().snapshot().performance.tickBudgetMs);
        session->saves().tickServer(event.level(), session->sessionGeneration(), budget);
        session->worldEdit().tickServer(event.level(), session->sessionGeneration(), budget);
    });
    gClientCommandListener = bus.emplaceListener<ll::event::command::ClientCommandRegisterEvent>(
    



    [](auto&) {
        if (auto session = accessSession(); session && session->usesClientCommands()) {
            command::registerCommands(true);
        }
    });
    gServerCommandListener = bus.emplaceListener<ll::event::command::ServerCommandRegisterEvent>(
    



    [](auto&) {
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
    auto lifecycle = gSessionGate.control();
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
