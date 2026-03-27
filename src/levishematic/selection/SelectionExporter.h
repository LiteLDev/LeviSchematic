#pragma once

#include "levishematic/selection/SelectionState.h"

#include <string_view>

class Dimension;

namespace levishematic::selection {

class SelectionExporter {
public:
    [[nodiscard]] bool saveToMcstructure(
        SelectionState const& state,
        std::string_view      name,
        Dimension&            dimension
    ) const;
};

} // namespace levishematic::selection
