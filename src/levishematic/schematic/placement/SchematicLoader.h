#pragma once

#include "levishematic/schematic/placement/PlacementTypes.h"

#include <filesystem>

namespace levishematic::placement {

class SchematicLoader {
public:
    [[nodiscard]] LoadAssetResult loadMcstructureAsset(
        std::filesystem::path const& path
    ) const;
};

} // namespace levishematic::placement
