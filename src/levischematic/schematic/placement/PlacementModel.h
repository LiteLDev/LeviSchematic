#pragma once

#include "levischematic/verifier/VerifierTypes.h"

#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <deque>

class StructureTemplate;

namespace levischematic::placement {

using PlacementId = uint32_t;





struct SchematicAsset {
    



    struct LocalBlockEntry {
        BlockPos                              localPos;
        const Block*                          renderBlock = nullptr;
    };

    BlockPos                     size{0, 0, 0};
    std::deque<LocalBlockEntry> localBlocks;
    std::string                  defaultName;
    std::shared_ptr<const StructureTemplate> structure;

    


    [[nodiscard]] int totalNonAirBlocks() const noexcept {
        return static_cast<int>(localBlocks.size());
    }
};





struct OverrideEntry {
    enum class Kind {
        Remove,
        SetBlock,
    };

    Kind                                 kind  = Kind::Remove;
    const Block*                         block = nullptr;
};





struct PlacementInstance {
    enum class Rotation : int {
        NONE   = 0,
        CW_90  = 1,
        CW_180 = 2,
        CCW_90 = 3,
    };

    enum class Mirror : int {
        NONE     = 0,
        MIRROR_X = 1,
        MIRROR_Z = 2,
    };

    PlacementId                          id = 0;
    std::shared_ptr<const SchematicAsset> asset;
    std::string                          name;
    std::filesystem::path                filePath;
    int                                  dimensionId   = 0;
    BlockPos                             origin{0, 0, 0};
    Rotation                             rotation      = Rotation::NONE;
    Mirror                               mirror        = Mirror::NONE;
    bool                                 enabled       = true;
    bool                                 renderEnabled = true;
    uint64_t                             revision      = 1;
    uint64_t                             contentRevision = 1;
    std::unordered_map<uint64_t, OverrideEntry> overrides;

    


    [[nodiscard]] int         totalNonAirBlocks() const noexcept;
    


    [[nodiscard]] std::string describeTransform() const;
};






[[nodiscard]] inline PlacementInstance::Rotation rotateBy(
    PlacementInstance::Rotation base,
    PlacementInstance::Rotation delta
) {
    return static_cast<PlacementInstance::Rotation>(
        (static_cast<int>(base) + static_cast<int>(delta)) % 4
    );
}








[[nodiscard]] inline BlockPos transformLocalPos(
    BlockPos const&             pos,
    BlockPos const&             size,
    PlacementInstance::Mirror   mirror,
    PlacementInstance::Rotation rot
) {
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;

    switch (mirror) {
    case PlacementInstance::Mirror::MIRROR_X:
        x = size.x - 1 - x;
        break;
    case PlacementInstance::Mirror::MIRROR_Z:
        z = size.z - 1 - z;
        break;
    case PlacementInstance::Mirror::NONE:
        break;
    }

    switch (rot) {
    case PlacementInstance::Rotation::CW_90:
        return {size.z - 1 - z, y, x};
    case PlacementInstance::Rotation::CW_180:
        return {size.x - 1 - x, y, size.z - 1 - z};
    case PlacementInstance::Rotation::CCW_90:
        return {z, y, size.x - 1 - x};
    case PlacementInstance::Rotation::NONE:
        return {x, y, z};
    }

    return {x, y, z};
}





[[nodiscard]] std::pair<BlockPos, BlockPos> computeEnclosingBox(PlacementInstance const& placement);

} 
