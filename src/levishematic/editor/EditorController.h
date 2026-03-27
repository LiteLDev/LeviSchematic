#pragma once

#include "levishematic/editor/EditorState.h"
#include "levishematic/schematic/placement/PlacementTypes.h"

#include <functional>
#include <memory>
#include <vector>

class Dimension;
class RenderChunkCoordinator;

namespace levishematic::placement {
class PlacementStore;
class SchematicLoader;
struct PlacementInstance;
}

namespace levishematic::render {
class ProjectionProjector;
struct ProjectionScene;
struct PatchOp;
}

namespace levishematic::selection {
class SelectionExporter;
struct SelectionOverlayView;
}

namespace levishematic::editor {

class EditorController {
public:
    EditorController(
        EditorState&                  state,
        placement::PlacementStore&    placementStore,
        placement::SchematicLoader&   schematicLoader,
        selection::SelectionExporter& selectionExporter,
        render::ProjectionProjector&  projector
    );

    [[nodiscard]] EditorState const& state() const { return mState; }
    [[nodiscard]] placement::PlacementStore&       placementStore() { return mPlacementStore; }
    [[nodiscard]] placement::PlacementStore const& placementStore() const { return mPlacementStore; }

    [[nodiscard]] placement::LoadPlacementResult loadSchematic(
        std::string const& filename,
        BlockPos           origin,
        std::string const& explicitName = {}
    );

    [[nodiscard]] placement::PlacementInstance const* findPlacement(placement::PlacementId id) const;
    [[nodiscard]] placement::PlacementInstance const* selectedPlacement() const;
    [[nodiscard]] std::optional<placement::PlacementId> selectedPlacementId() const;
    [[nodiscard]] std::vector<std::reference_wrapper<placement::PlacementInstance const>> orderedPlacements() const;

    [[nodiscard]] bool removePlacement(placement::PlacementId id);
    void               clearPlacements();
    [[nodiscard]] bool selectPlacement(placement::PlacementId id);
    [[nodiscard]] bool togglePlacementEnabled(placement::PlacementId id);
    [[nodiscard]] bool togglePlacementRender(placement::PlacementId id);
    [[nodiscard]] bool movePlacement(placement::PlacementId id, int dx, int dy, int dz);
    [[nodiscard]] bool setPlacementOrigin(placement::PlacementId id, BlockPos origin);
    [[nodiscard]] bool rotatePlacement(
        placement::PlacementId id,
        placement::PlacementInstance::Rotation delta
    );
    [[nodiscard]] bool setPlacementMirror(
        placement::PlacementId id,
        placement::PlacementInstance::Mirror mirror
    );
    [[nodiscard]] bool resetPlacementTransform(placement::PlacementId id);
    [[nodiscard]] bool patchPlacementBlock(
        placement::PlacementId id,
        BlockPos               worldPos,
        render::PatchOp const& op
    );

    void setSelectionCorner1(BlockPos pos);
    void setSelectionCorner2(BlockPos pos);
    void clearSelection();
    void setSelectionMode(bool enabled);
    void toggleSelectionMode();

    [[nodiscard]] bool saveSelection(std::string_view name, Dimension& dimension) const;
    [[nodiscard]] selection::SelectionOverlayView selectionOverlay() const;

    [[nodiscard]] bool flushProjectionRefresh(
        std::shared_ptr<RenderChunkCoordinator> const& coordinator
    );
    [[nodiscard]] std::shared_ptr<const render::ProjectionScene> projectionScene() const;
    void clearProjection();

private:
    void touchSelectionState();

    EditorState&                  mState;
    placement::PlacementStore&    mPlacementStore;
    placement::SchematicLoader&   mSchematicLoader;
    selection::SelectionExporter& mSelectionExporter;
    render::ProjectionProjector&  mProjector;
};

} // namespace levishematic::editor
