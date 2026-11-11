#pragma once

#include "LayerRange.h"
#include "levischematic/schematic/placement/SchematicContent.h"
#include "levischematic/schematic/placement/ResourceState.h"

#include "levischematic/schematic/placement/PlacementStore.h"
#include "levischematic/selection/SelectionState.h"
#include "levischematic/verifier/VerifierTypes.h"

namespace levischematic::editor {

enum class ToolMode {
    None,
    Placement,
    Selection,
    Edit,
    Paste,
    GridPaste,
    Fill,
    Replace,
    Delete,
    Clone,
    Move,
};
inline bool creativeTool(ToolMode mode) { return mode >= ToolMode::Paste && mode <= ToolMode::Move; }
inline bool areaTool(ToolMode mode) { return mode == ToolMode::Selection || mode >= ToolMode::Fill; }










struct ToolState {
    ToolMode activeTool = ToolMode::Selection;
    bool     enabled    = true;
    uint64_t revision   = 0;
    Block const* sampledBlock = nullptr;
    placement::EditMatch editMatch = placement::EditMatch::Single;
    bool editAdjacent = true;
};





struct ViewState {
    LayerRange layerRange;
    uint64_t   revision = 0;
};





struct DebugState {
    uint64_t revision = 0;
};





struct EditorState {
    placement::ResourceState resources;
    placement::PlacementState placements;
    selection::SelectionState selection;
    verifier::VerifierState   verifier;
    ToolState                 tool;
    ViewState                 view;
    DebugState                debug;
};

} 
