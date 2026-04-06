#pragma once

#include "levishematic/app/PlacementService.h"
#include "levishematic/app/ProjectionService.h"
#include "levishematic/app/RuntimeContext.h"
#include "levishematic/app/SelectionService.h"
#include "levishematic/input/InputHandler.h"
#include "levishematic/verifier/VerifierService.h"

#include <memory>

namespace levishematic::placement {
class PlacementStore;
class SchematicLoader;
}

namespace levishematic::render {
class ProjectionProjector;
}

namespace levishematic::selection {
class SelectionExporter;
}

namespace levishematic::app {

class AppKernel {
public:
    AppKernel();
    ~AppKernel();

    void initialize();
    void shutdown();
    void onServerThreadStarted();

    [[nodiscard]] RuntimeContext&          runtime() { return mRuntime; }
    [[nodiscard]] PlacementService&        placement() { return *mPlacementService; }
    [[nodiscard]] PlacementService const&  placement() const { return *mPlacementService; }
    [[nodiscard]] SelectionService&        selection() { return *mSelectionService; }
    [[nodiscard]] SelectionService const&  selection() const { return *mSelectionService; }
    [[nodiscard]] ProjectionService&       projection() { return *mProjectionService; }
    [[nodiscard]] ProjectionService const& projection() const { return *mProjectionService; }
    [[nodiscard]] verifier::VerifierService& verifier() { return *mVerifierService; }
    [[nodiscard]] verifier::VerifierService const& verifier() const { return *mVerifierService; }
    [[nodiscard]] input::InputHandler&     input() { return mInputHandler; }

private:
    void configureSchematicDirectory();

    RuntimeContext                                mRuntime;
    std::unique_ptr<editor::EditorState>          mState;
    std::unique_ptr<placement::PlacementStore>    mPlacementStore;
    std::unique_ptr<placement::SchematicLoader>   mSchematicLoader;
    std::unique_ptr<selection::SelectionExporter> mSelectionExporter;
    std::unique_ptr<render::ProjectionProjector>  mProjector;
    std::unique_ptr<PlacementService>             mPlacementService;
    std::unique_ptr<SelectionService>             mSelectionService;
    std::unique_ptr<ProjectionService>            mProjectionService;
    std::unique_ptr<verifier::VerifierService>    mVerifierService;
    input::InputHandler                           mInputHandler;
    bool                                          mInitialized        = false;
    bool                                          mCommandsRegistered = false;
};

[[nodiscard]] bool       hasAppKernel();
[[nodiscard]] AppKernel& getAppKernel();
void                     start();
void                     stop();

} // namespace levishematic::app
