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

namespace levischematic::placement {

using PlacementId = uint32_t;

struct SchematicAsset {
    struct LocalBlockEntry {
        BlockPos                              localPos;
        const Block*                          renderBlock = nullptr;
        verifier::BlockCompareSpec            compareSpec;
        std::optional<verifier::BlockEntitySnapshot> blockEntity;
    };

    BlockPos                     size{0, 0, 0};
    std::vector<LocalBlockEntry> localBlocks;
    std::string                  defaultName;

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
    std::optional<verifier::BlockEntitySnapshot> blockEntity;
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
    std::unordered_map<uint64_t, OverrideEntry> overrides;

    [[nodiscard]] int         totalNonAirBlocks() const noexcept;
    [[nodiscard]] std::string describeTransform() const;
};

[[nodiscard]] PlacementInstance::Rotation rotateBy(
    PlacementInstance::Rotation base,
    PlacementInstance::Rotation delta
);

[[nodiscard]] BlockPos transformLocalPos(
    BlockPos const&              pos,
    BlockPos const&              size,
    PlacementInstance::Mirror    mirror,
    PlacementInstance::Rotation  rot
);

[[nodiscard]] std::pair<BlockPos, BlockPos> computeEnclosingBox(PlacementInstance const& placement);

} // namespace levischematic::placement
