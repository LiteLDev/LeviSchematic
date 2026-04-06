#include "AppKernel.h"

#include "levishematic/command/Command.h"
#include "levishematic/hook/RuntimeHooks.h"
#include "levishematic/render/ProjectionRenderer.h"
#include "levishematic/schematic/placement/PlacementStore.h"
#include "levishematic/schematic/placement/SchematicLoader.h"
#include "levishematic/selection/SelectionExporter.h"

#include "ll/api/service/ServerInfo.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"

namespace levishematic::app {
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
    , mProjectionService(std::make_unique<ProjectionService>(
          mState->placements,
          mState->verifier,
          *mProjector
      ))
    , mVerifierService(std::make_unique<verifier::VerifierService>(
          mState->verifier,
          mState->placements,
          *mProjector
      )) {}

AppKernel::~AppKernel() = default;

void AppKernel::initialize() {
    if (mInitialized) {
        return;
    }

    configureSchematicDirectory();
    hook::registerRuntimeHooks();
    // mVerifierService->attachToRuntime();
    mInitialized = true;
}

void AppKernel::shutdown() {
    if (!mInitialized) {
        return;
    }

    mInputHandler.shutdown();
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

bool hasAppKernel() {
    return static_cast<bool>(gAppKernel);
}

AppKernel& getAppKernel() {
    return *gAppKernel;
}

void start() {
    if (!gAppKernel) {
        gAppKernel = std::make_unique<AppKernel>();
    }
    gAppKernel->initialize();
}

void stop() {
    if (!gAppKernel) {
        return;
    }

    gAppKernel->shutdown();
    gAppKernel.reset();
}

static bool reg = [] {
    using namespace ll::event;
    EventBus::getInstance().emplaceListener<ClientJoinLevelEvent>([](ClientJoinLevelEvent&) {
        getAppKernel().verifier().attachToRuntime();
    });
    return true;
}();

} // namespace levishematic::app
