#include "AppKernel.h"

#include "levischematic/command/Command.h"
#include "levischematic/hook/RuntimeHooks.h"
#include "levischematic/app/ProjectionRefresh.h"
#include "levischematic/input/InputHandler.h"
#include "levischematic/render/ProjectionRenderer.h"
#include "levischematic/schematic/placement/PlacementStore.h"
#include "levischematic/schematic/placement/SchematicLoader.h"
#include "levischematic/selection/SelectionExporter.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/ServerInfo.h"

namespace levischematic::app {
namespace {

std::unique_ptr<AppKernel> gAppKernel;

} // namespace

AppKernel::AppKernel()
    : mState(std::make_unique<editor::EditorState>())
    , mPlacementStore(std::make_unique<placement::PlacementStore>(mState->placements))
    , mSchematicLoader(std::make_unique<placement::SchematicLoader>())
    , mSelectionExporter(std::make_unique<selection::SelectionExporter>())
    , mProjector(std::make_unique<render::ProjectionProjector>())
    , mPlacementService(std::make_unique<PlacementService>(
          *mState,
          mRuntime,
          *mPlacementStore,
          *mSchematicLoader
      ))
    , mSelectionService(std::make_unique<SelectionService>(
          *mState,
          mRuntime,
          *mSelectionExporter
      ))
    , mInfoOverlayService(std::make_unique<InfoOverlayService>(
          *mPlacementService,
          *mSelectionService
      ))
    , mViewService(std::make_unique<ViewService>(mState->view))
    , mProjectionService(std::make_unique<ProjectionService>(
          mState->placements,
          mState->verifier,
          mState->view,
          *mProjector
      ))
    , mVerifierService(std::make_unique<verifier::VerifierService>(
          mState->verifier,
          mState->placements,
          mState->view,
          *mProjector
      )) {
        mInputHandler = input::InputHandler();
        mInputHandler.initialize({
            .viewService            = mViewService.get(),
            .projectionService      = mProjectionService.get(),
            .verifierService        = mVerifierService.get(),
            .refreshProjectionState = [] {
                (void)refreshCurrentClientProjectionState();
            },
        });
    }

AppKernel::~AppKernel() = default;

void AppKernel::initialize() {
    if (mInitialized) {
        return;
    }

    configureSchematicDirectory();
    hook::registerRuntimeHooks();
    registerLifecycleListeners();
    mInitialized = true;
}

void AppKernel::shutdown() {
    if (!mInitialized) {
        return;
    }

    mInputHandler.shutdown();
    unregisterLifecycleListeners();
    hook::unregisterRuntimeHooks();
    mVerifierService->detachFromRuntime();
    mSelectionService->clearSelection();
    mPlacementService->clearPlacements();
    mVerifierService->clear();
    mProjectionService->clear();
    mCommandsRegistered = false;
    mInitialized        = false;
}

void AppKernel::onServerThreadStarted() {
    if (!mInitialized || mCommandsRegistered) {
        return;
    }

    command::registerCommands(false);
    mCommandsRegistered = true;
}

void AppKernel::configureSchematicDirectory() {
    std::filesystem::path structurePath;
    structurePath /= ll::getWorldPath().value();
    structurePath  = structurePath.parent_path().parent_path();
    structurePath /= "schematics";

    std::error_code ec;
    std::filesystem::create_directories(structurePath, ec);

    mRuntime.setSchematicDirectory(structurePath);
}

void AppKernel::registerLifecycleListeners() {
    auto& bus = ll::event::EventBus::getInstance();

    if (!mClientJoinListener) {
        mClientJoinListener = bus.emplaceListener<ll::event::client::ClientJoinLevelEvent>(
            [this](ll::event::client::ClientJoinLevelEvent&) { mVerifierService->handleJoinLevel(); }
        );
    }

    if (!mClientExitListener) {
        mClientExitListener = bus.emplaceListener<ll::event::client::ClientExitLevelEvent>(
            [this](ll::event::client::ClientExitLevelEvent&) { mVerifierService->handleExitLevel(); }
        );
    }

    if (!mLevelTickListener) {
        mLevelTickListener = bus.emplaceListener<ll::event::world::ServerLevelTickEvent>(
            [this](ll::event::world::ServerLevelTickEvent&) { mVerifierService->ensureRuntimeBindings(); }
        );
    }
}

void AppKernel::unregisterLifecycleListeners() {
    auto& bus = ll::event::EventBus::getInstance();

    if (mClientJoinListener) {
        bus.removeListener<ll::event::client::ClientJoinLevelEvent>(mClientJoinListener);
        mClientJoinListener.reset();
    }

    if (mClientExitListener) {
        bus.removeListener<ll::event::client::ClientExitLevelEvent>(mClientExitListener);
        mClientExitListener.reset();
    }

    if (mLevelTickListener) {
        bus.removeListener<ll::event::world::ServerLevelTickEvent>(mLevelTickListener);
        mLevelTickListener.reset();
    }
}

bool hasAppKernel() {
    return static_cast<bool>(gAppKernel);
}

AppKernel& getAppKernel() {
    return *gAppKernel;
}

void load() {
    if (!gAppKernel) {
        gAppKernel = std::make_unique<AppKernel>();
    }
}

void start() {
    gAppKernel->initialize();
}

void stop() {
    if (!gAppKernel) {
        return;
    }

    gAppKernel->shutdown();
    gAppKernel.reset();
}

} // namespace levischematic::app
