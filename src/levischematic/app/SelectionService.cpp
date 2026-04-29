#include "SelectionService.h"

namespace levischematic::app {

std::string SelectionError::describe(std::string_view target) const {
    switch (code) {
    case Code::IncompleteSelection:
        return "Selection is incomplete. Set both corners first (pos1/pos2).";
    case Code::RuntimeUnavailable:
        return detail.empty() ? "Schematic directory is not configured." : detail;
    case Code::SaveFailed:
        if (!detail.empty()) {
            return detail;
        }
        return target.empty()
            ? "Failed to save selection."
            : "Failed to save selection as '" + std::string(target) + "'.";
    }

    return "Unknown selection error.";
}

SelectionService::SelectionService(
    editor::EditorState&          state,
    RuntimeContext&               runtime,
    selection::SelectionExporter& selectionExporter
)
    : mState(state)
    , mRuntime(runtime)
    , mSelectionExporter(selectionExporter) {}

bool SelectionService::isSelectionModeEnabled() const {
    return mState.selection.selectionMode;
}

selection::SelectionOverlayView SelectionService::overlay() const {
    return selection::makeOverlayView(mState.selection);
}

void SelectionService::setSelectionCorner1(BlockPos pos) {
    mState.selection.corner1 = pos;
    touchSelectionState();
}

void SelectionService::setSelectionCorner2(BlockPos pos) {
    mState.selection.corner2 = pos;
    touchSelectionState();
}

void SelectionService::clearSelection() {
    mState.selection.corner1.reset();
    mState.selection.corner2.reset();
    touchSelectionState();
}

void SelectionService::setSelectionMode(bool enabled) {
    mState.selection.selectionMode = enabled;
    touchSelectionState();
}

void SelectionService::toggleSelectionMode() {
    mState.selection.selectionMode = !mState.selection.selectionMode;
    touchSelectionState();
}

Result<SelectionSaveInfo, SelectionError> SelectionService::saveSelection(
    std::string_view name,
    Dimension&       dimension
) const {
    if (!selection::hasCompleteSelection(mState.selection)) {
        return Result<SelectionSaveInfo, SelectionError>::failure({
            .code = SelectionError::Code::IncompleteSelection,
        });
    }

    if (!mRuntime.hasSchematicDirectory()) {
        return Result<SelectionSaveInfo, SelectionError>::failure({
            .code   = SelectionError::Code::RuntimeUnavailable,
            .detail = "Schematic directory is not configured.",
        });
    }

    auto exportResult = mSelectionExporter.saveToMcstructure(
        mState.selection,
        mRuntime.schematicDirectory(),
        name,
        dimension
    );
    if (!exportResult) {
        return Result<SelectionSaveInfo, SelectionError>::failure({
            .code   = SelectionError::Code::SaveFailed,
            .path   = exportResult.error().path,
            .detail = exportResult.error().describe(name),
        });
    }

    return Result<SelectionSaveInfo, SelectionError>::success({
        .name = std::string(name),
        .size = selection::getSize(mState.selection),
        .path = exportResult.value(),
    });
}

void SelectionService::touchSelectionState() {
    ++mState.selection.revision;
}

} // namespace levischematic::app
