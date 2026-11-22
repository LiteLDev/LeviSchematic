#pragma once
#include "levischematic/schematic/placement/model/PlacementTypes.h"
#include "levischematic/shared/scheduling/WorkBudget.h"
#include <stop_token>

class Level;

namespace levischematic::placement {
struct AssetPreparation {
    std::vector<AssetPreparation>   regions;
    size_t                          regionCursor = 0;
    std::shared_ptr<SchematicAsset> asset;
    std::vector<Block const*>       palette;
    std::size_t                     paletteCursor = 0;
    std::size_t                     blockCursor   = 0;
    std::string                     notice;
};
class SchematicLoader {
public:
    static Expected<AssetPreparation, LoadPlacementError> read(std::filesystem::path const& path, std::stop_token stop);
    static Expected<AssetPreparation, LoadPlacementError>
                readTag(CompoundTag const&, std::filesystem::path const&, std::stop_token);
    static bool advance(AssetPreparation& pending, Level& level, util::WorkBudget const& budget);
};
} // namespace levischematic::placement
