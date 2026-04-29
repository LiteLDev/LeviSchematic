#pragma once

#include "levischematic/schematic/placement/PlacementTypes.h"

#include <filesystem>

namespace levischematic::placement {

class SchematicLoader {
public:
    [[nodiscard]] LoadAssetResult loadMcstructureAsset(
        std::filesystem::path const& path
    ) const;
};

} // namespace levischematic::placement
