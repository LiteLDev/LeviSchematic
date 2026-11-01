#pragma once
#include "PlacementTypes.h"
#include "levischematic/util/WorkBudget.h"
#include <stop_token>

namespace levischematic::placement {

struct AssetPreparation {
    std::shared_ptr<SchematicAsset> asset;
    std::vector<Block const*>       palette;
    std::size_t                     paletteCursor = 0;
    std::size_t                     blockCursor   = 0;
};

class SchematicLoader {
public:
    static Expected<AssetPreparation, LoadPlacementError> read(std::filesystem::path const& path, std::stop_token stop);

    static bool advance(AssetPreparation& pending, util::WorkBudget const& budget);
};
} 
