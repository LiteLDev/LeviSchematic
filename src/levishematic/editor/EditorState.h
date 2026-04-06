#pragma once

#include "levishematic/schematic/placement/PlacementStore.h"
#include "levishematic/selection/SelectionState.h"
#include "levishematic/verifier/VerifierTypes.h"

namespace levishematic::editor {

enum class ToolMode {
    None,
    Placement,
    Selection,
};

struct LayerRange {
    int  minY    = -64;
    int  maxY    = 320;
    bool enabled = false;
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

} // namespace levishematic::editor
