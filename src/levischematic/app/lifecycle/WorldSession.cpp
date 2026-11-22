#include "WorldSession.h"
namespace levischematic::app {
void AppKernel::WorldSession::configureSchematicDirectory() {
    if (runtime.hasSchematicDirectory()) return;
    auto            worldPath = ll::getWorldPath();
    auto            directory = worldPath ? worldPath->parent_path().parent_path() / "schematics"
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
AppKernel::WorldSession::WorldSession()
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
} // namespace levischematic::app
