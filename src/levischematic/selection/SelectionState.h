#pragma once

#include "levischematic/selection/Box.h"
#include "levischematic/selection/ToolGeometry.h"

#include "mc/world/level/BlockPos.h"

#include <optional>
#include <string>
#include <vector>

namespace levischematic::selection {
struct SelectionRegion {
    uint32_t id = 0;
    std::string name;
    std::optional<BlockPos> corner1,corner2;
};





struct SelectionState {
    bool normal = false;
    uint32_t selectedRegion = 0, nextRegion = 1;
    std::vector<SelectionRegion> regions;
    std::optional<BlockPos> customOrigin;
    std::optional<BlockPos> corner1;
    std::optional<BlockPos> corner2;
    bool                    selectionMode = true;
    tool::Element           selectedElement = tool::Element::Box;
    tool::CornerMode        cornerMode      = tool::CornerMode::Corners;
    uint64_t                revision      = 0;
};





struct SelectionOverlayView {
    bool                    selectionMode = true;
    std::optional<BlockPos> corner1;
    std::optional<BlockPos> corner2;
    std::optional<BlockPos> origin;
    std::optional<BlockPos> size;
};





[[nodiscard]] bool               hasCompleteSelection(SelectionState const& state);




[[nodiscard]] std::optional<Box> getSelectionBox(SelectionState const& state);




[[nodiscard]] BlockPos           getMinCorner(SelectionState const& state);




[[nodiscard]] BlockPos           getSize(SelectionState const& state);




[[nodiscard]] SelectionOverlayView makeOverlayView(SelectionState const& state);

} 
