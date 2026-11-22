#include "levischematic/schematic/placement/model/PlacementModel.h"
#include "levischematic/schematic/placement/model/PlacementGeometry.h"

#include "levischematic/schematic/placement/model/VolumeIndex.h"
#include "levischematic/shared/geometry/PositionUtils.h"

#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include "mc/world/level/levelgen/structure/StructureTemplateData.h"

#include <algorithm>

namespace levischematic::placement {

int PlacementInstance::totalNonAirBlocks() const noexcept { return asset ? asset->totalNonAirBlocks() : 0; }

Block const* secondaryLayerBlock(SchematicAsset const& asset, BlockPos const& local) {
    if (!asset.structure || local.x < 0 || local.y < 0 || local.z < 0 || local.x >= asset.size.x
        || local.y >= asset.size.y || local.z >= asset.size.z)
        return nullptr;
    auto const& extra   = asset.structure->mStructureTemplateData->mExtraBlockIndices.get();
    auto const  index   = volumeIndex({local.x, local.y, local.z}, {asset.size.x, asset.size.y, asset.size.z});
    auto const  palette = secondaryLayerIndex(extra ? &*extra : nullptr, index, asset.editedCells.contains(index));
    if (!palette || *palette >= asset.palette.size()) return nullptr;
    auto const* block = asset.palette[*palette];
    return block && !block->isAir() ? block : nullptr;
}

std::string PlacementInstance::describeTransform() const {
    static constexpr const char* kRotationNames[] = {"0deg", "CW90", "180deg", "CCW90"};
    static constexpr const char* kMirrorNames[]   = {"None", "X", "Z"};

    return "Origin:(" + std::to_string(origin.x) + "," + std::to_string(origin.y) + "," + std::to_string(origin.z)
         + ") Rot:" + kRotationNames[static_cast<int>(rotation)] + " Mir:" + kMirrorNames[static_cast<int>(mirror)];
}

std::pair<BlockPos, BlockPos> computeEnclosingBox(PlacementInstance const& placement) {
    if (!placement.asset || placement.asset->localBlocks.empty()) {
        return {placement.origin, placement.origin};
    }

    std::unordered_map<uint64_t, BlockPos> resolvedPositions;
    resolvedPositions.reserve(placement.asset->localBlocks.size() + placement.overrides.size());

    auto applyResolvedPos = [&](BlockPos const& worldPos) {
        resolvedPositions[util::encodePosKey(worldPos)] = worldPos;
    };

    for (auto const& entry : placement.asset->localBlocks) {
        if (!entry.renderBlock || entry.renderBlock->isAir()) {
            continue;
        }

        auto     local = transformLocalPos(entry.localPos, placement.asset->size, placement.mirror, placement.rotation);
        BlockPos worldPos{
            placement.origin.x + local.x,
            placement.origin.y + local.y,
            placement.origin.z + local.z,
        };

        auto posKey = util::encodePosKey(worldPos);
        if (auto overrideIt = placement.overrides.find(util::encodePosKey(entry.localPos));
            overrideIt != placement.overrides.end()) {
            if (overrideIt->second.kind == OverrideEntry::Kind::Remove) {
                resolvedPositions.erase(posKey);
                continue;
            }
        }

        applyResolvedPos(worldPos);
    }

    for (auto const& [posKey, overrideEntry] : placement.overrides) {
        if (overrideEntry.kind == OverrideEntry::Kind::SetBlock && overrideEntry.block != nullptr) {
            if (auto world = toWorld(placement, util::decodePosKey(posKey))) applyResolvedPos(*world);
        }
    }

    if (resolvedPositions.empty()) {
        return {placement.origin, placement.origin};
    }

    auto it     = resolvedPositions.begin();
    auto minPos = it->second;
    auto maxPos = it->second;
    ++it;

    for (; it != resolvedPositions.end(); ++it) {
        auto const& worldPos = it->second;
        minPos.x             = std::min(minPos.x, worldPos.x);
        minPos.y             = std::min(minPos.y, worldPos.y);
        minPos.z             = std::min(minPos.z, worldPos.z);
        maxPos.x             = std::max(maxPos.x, worldPos.x);
        maxPos.y             = std::max(maxPos.y, worldPos.y);
        maxPos.z             = std::max(maxPos.z, worldPos.z);
    }

    return {minPos, maxPos};
}

} // namespace levischematic::placement
