#pragma once

#include "levishematic/app/Result.h"
#include "levishematic/app/RuntimeContext.h"
#include "levishematic/editor/EditorState.h"
#include "levishematic/selection/SelectionExporter.h"

#include <string_view>

class Dimension;

namespace levishematic::app {

struct SelectionError {
    enum class Code {
        IncompleteSelection,
        RuntimeUnavailable,
        SaveFailed,
    };

    Code                  code = Code::SaveFailed;
    std::filesystem::path path;
    std::string           detail;

    [[nodiscard]] std::string describe(std::string_view target = {}) const;
};

struct SelectionSaveInfo {
    std::string           name;
    BlockPos              size;
    std::filesystem::path path;
};

class SelectionService {
public:
    SelectionService(
        editor::EditorState&          state,
        RuntimeContext&               runtime,
        selection::SelectionExporter& selectionExporter
    );

    [[nodiscard]] selection::SelectionState const& state() const { return mState.selection; }
    [[nodiscard]] bool                             isSelectionModeEnabled() const;
    [[nodiscard]] selection::SelectionOverlayView  overlay() const;

    void setSelectionCorner1(BlockPos pos);
    void setSelectionCorner2(BlockPos pos);
    void clearSelection();
    void setSelectionMode(bool enabled);
    void toggleSelectionMode();

    [[nodiscard]] Result<SelectionSaveInfo, SelectionError> saveSelection(
        std::string_view name,
        Dimension&       dimension
    ) const;

private:
    void touchSelectionState();

    editor::EditorState&          mState;
    RuntimeContext&               mRuntime;
    selection::SelectionExporter& mSelectionExporter;
};

} // namespace levishematic::app
