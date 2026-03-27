#include "EditorController.h"

#include "levishematic/render/ProjectionRenderer.h"
#include "levishematic/schematic/placement/PlacementStore.h"
#include "levishematic/schematic/placement/SchematicLoader.h"
#include "levishematic/selection/SelectionExporter.h"

namespace levishematic::editor {

EditorController::EditorController(
    EditorState&                  state,
    placement::PlacementStore&    placementStore,
    placement::SchematicLoader&   schematicLoader,
    selection::SelectionExporter& selectionExporter,
    render::ProjectionProjector&  projector
)
    : mState(state)
    , mPlacementStore(placementStore)
    , mSchematicLoader(schematicLoader)
    , mSelectionExporter(selectionExporter)
    , mProjector(projector) {}

placement::LoadPlacementResult EditorController::loadSchematic(
    std::string const& filename,
    BlockPos           origin,
    std::string const& explicitName
) {
    auto resolvedPath = mPlacementStore.resolveSchematicPath(filename);
    auto assetResult  = mSchematicLoader.loadMcstructureAsset(resolvedPath);
    if (!assetResult) {
        return placement::LoadPlacementResult{
            .id           = std::nullopt,
            .resolvedPath = resolvedPath,
            .error        = assetResult.error(),
        };
    }

    auto displayName = explicitName.empty() ? assetResult.value()->defaultName : explicitName;
    auto placementId = mPlacementStore.createPlacement(
        assetResult.value(),
        origin,
        displayName,
        resolvedPath
    );

    return placement::LoadPlacementResult{
        .id           = placementId,
        .resolvedPath = resolvedPath,
        .error        = std::nullopt,
    };
}

placement::PlacementInstance const* EditorController::findPlacement(placement::PlacementId id) const {
    return mPlacementStore.get(id);
}

placement::PlacementInstance const* EditorController::selectedPlacement() const {
    return mPlacementStore.selected();
}

std::optional<placement::PlacementId> EditorController::selectedPlacementId() const {
    return mState.placements.selectedId;
}

std::vector<std::reference_wrapper<placement::PlacementInstance const>> EditorController::orderedPlacements() const {
    std::vector<std::reference_wrapper<placement::PlacementInstance const>> placements;
    placements.reserve(mState.placements.order.size());

    for (auto placementId : mState.placements.order) {
        if (auto const* placement = mPlacementStore.get(placementId)) {
            placements.emplace_back(*placement);
        }
    }

    return placements;
}

bool EditorController::removePlacement(placement::PlacementId id) {
    return mPlacementStore.remove(id);
}

void EditorController::clearPlacements() {
    mPlacementStore.clear();
}

bool EditorController::selectPlacement(placement::PlacementId id) {
    return mPlacementStore.select(id);
}

bool EditorController::togglePlacementEnabled(placement::PlacementId id) {
    return mPlacementStore.toggleEnabled(id);
}

bool EditorController::togglePlacementRender(placement::PlacementId id) {
    return mPlacementStore.toggleRender(id);
}

bool EditorController::movePlacement(placement::PlacementId id, int dx, int dy, int dz) {
    return mPlacementStore.move(id, dx, dy, dz);
}

bool EditorController::setPlacementOrigin(placement::PlacementId id, BlockPos origin) {
    return mPlacementStore.setOrigin(id, origin);
}

bool EditorController::rotatePlacement(
    placement::PlacementId                 id,
    placement::PlacementInstance::Rotation delta
) {
    return mPlacementStore.rotate(id, delta);
}

bool EditorController::setPlacementMirror(
    placement::PlacementId               id,
    placement::PlacementInstance::Mirror mirror
) {
    return mPlacementStore.setMirror(id, mirror);
}

bool EditorController::resetPlacementTransform(placement::PlacementId id) {
    return mPlacementStore.resetTransform(id);
}

bool EditorController::patchPlacementBlock(
    placement::PlacementId id,
    BlockPos               worldPos,
    render::PatchOp const& op
) {
    return mPlacementStore.patchBlock(id, worldPos, op);
}

void EditorController::setSelectionCorner1(BlockPos pos) {
    mState.selection.corner1 = pos;
    touchSelectionState();
}

void EditorController::setSelectionCorner2(BlockPos pos) {
    mState.selection.corner2 = pos;
    touchSelectionState();
}

void EditorController::clearSelection() {
    mState.selection.corner1.reset();
    mState.selection.corner2.reset();
    touchSelectionState();
}

void EditorController::setSelectionMode(bool enabled) {
    mState.selection.selectionMode = enabled;
    touchSelectionState();
}

void EditorController::toggleSelectionMode() {
    mState.selection.selectionMode = !mState.selection.selectionMode;
    touchSelectionState();
}

bool EditorController::saveSelection(std::string_view name, Dimension& dimension) const {
    return mSelectionExporter.saveToMcstructure(mState.selection, name, dimension);
}

selection::SelectionOverlayView EditorController::selectionOverlay() const {
    return selection::makeOverlayView(mState.selection);
}

bool EditorController::flushProjectionRefresh(
    std::shared_ptr<RenderChunkCoordinator> const& coordinator
) {
    if (!mProjector.needsRefresh(mState.placements.revision)) {
        return false;
    }

    mProjector.rebuildAndRefresh(mState.placements, coordinator);
    return true;
}

std::shared_ptr<const render::ProjectionScene> EditorController::projectionScene() const {
    return mProjector.scene();
}

void EditorController::clearProjection() {
    mProjector.clear();
}

void EditorController::touchSelectionState() {
    ++mState.selection.revision;
}

} // namespace levishematic::editor
