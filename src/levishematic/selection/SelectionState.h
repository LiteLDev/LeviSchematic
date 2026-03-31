#pragma once

#include "levishematic/selection/Box.h"

#include "mc/world/level/BlockPos.h"

#include <optional>

namespace levishematic::selection {

struct SelectionState {
    std::optional<BlockPos> corner1;
    std::optional<BlockPos> corner2;
    bool                    selectionMode = true;
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

} // namespace levishematic::selection
