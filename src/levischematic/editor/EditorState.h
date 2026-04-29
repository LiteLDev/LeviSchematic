#pragma once

#include "levischematic/schematic/placement/PlacementStore.h"
#include "levischematic/selection/SelectionState.h"
#include "levischematic/verifier/VerifierTypes.h"

namespace levischematic::editor {

enum class ToolMode {
    None,
    Placement,
    Selection,
};

struct LayerRange {
    int  minY    = -64;
    int  maxY    = 320;
    bool enabled = false;

    [[nodiscard]] bool contains(int worldY) const noexcept {
        return !enabled || (worldY >= minY && worldY <= maxY);
    }
};

struct ToolState {
    ToolMode activeTool = ToolMode::None;
    uint64_t revision   = 0;
};

struct ViewState {
    LayerRange layerRange;
    uint64_t   revision = 0;
};

struct DebugState {
    uint64_t revision = 0;
};

struct EditorState {
    placement::PlacementState placements;
    selection::SelectionState selection;
    verifier::VerifierState   verifier;
    ToolState                 tool;
    ViewState                 view;
    DebugState                debug;
};

} // namespace levischematic::editor
