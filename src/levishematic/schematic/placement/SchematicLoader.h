#pragma once

#include "levishematic/schematic/placement/PlacementTypes.h"

#include "mc/world/level/BlockPos.h"

#include <filesystem>
#include <string>

namespace levishematic::placement {

class SchematicLoader {
public:
    LoadPlacementDataResult loadMcstructurePlacement(
        const std::filesystem::path& path,
        BlockPos                      origin,
        const std::string&            name
    ) const;
};

} // namespace levishematic::placement

