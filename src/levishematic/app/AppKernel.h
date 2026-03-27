#pragma once

#include "levishematic/editor/EditorController.h"
#include "levishematic/input/InputHandler.h"

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

    [[nodiscard]] editor::EditorController&       controller() { return *mController; }
    [[nodiscard]] editor::EditorController const& controller() const { return *mController; }
    [[nodiscard]] input::InputHandler&            input() { return mInputHandler; }

private:
    void configureSchematicDirectory();

    std::unique_ptr<editor::EditorState>           mState;
    std::unique_ptr<placement::PlacementStore>     mPlacementStore;
    std::unique_ptr<placement::SchematicLoader>    mSchematicLoader;
    std::unique_ptr<selection::SelectionExporter>  mSelectionExporter;
    std::unique_ptr<render::ProjectionProjector>   mProjector;
    std::unique_ptr<editor::EditorController>      mController;
    input::InputHandler                            mInputHandler;
    bool                                           mInitialized        = false;
    bool                                           mCommandsRegistered = false;
};

[[nodiscard]] bool       hasAppKernel();
[[nodiscard]] AppKernel& getAppKernel();
void                     start();
void                     stop();

} // namespace levishematic::app
