#include "PlacementModel.h"

#include "levischematic/util/PositionUtils.h"

#include <algorithm>

namespace levischematic::placement {

int PlacementInstance::totalNonAirBlocks() const noexcept { return asset ? asset->totalNonAirBlocks() : 0; }

std::string PlacementInstance::describeTransform() const {
    static constexpr char const* kRotationNames[] = {"0deg", "CW90", "180deg", "CCW90"};
    static constexpr char const* kMirrorNames[]   = {"None", "X", "Z"};

    return "Origin:(" + std::to_string(origin.x) + "," + std::to_string(origin.y) + "," + std::to_string(origin.z)
         + ") Rot:" + kRotationNames[static_cast<int>(rotation)] + " Mir:" + kMirrorNames[static_cast<int>(mirror)];
}

std::pair<BlockPos, BlockPos> computeEnclosingBox(PlacementInstance const& placement) {
    if (!placement.asset || placement.asset->localBlocks.empty()) {
        return {placement.origin, placement.origin};
    }

    std::unordered_map<uint64_t, BlockPos> resolvedPositions;
    resolvedPositions.reserve(placement.asset->localBlocks.size() + placement.overrides.size());

    auto applyResolvedPos =


        [&](BlockPos const& worldPos) { resolvedPositions[util::encodePosKey(worldPos)] = worldPos; };

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
        if (auto overrideIt = placement.overrides.find(posKey); overrideIt != placement.overrides.end()) {
            if (overrideIt->second.kind == OverrideEntry::Kind::Remove) {
                resolvedPositions.erase(posKey);
                continue;
            }
        }

        applyResolvedPos(worldPos);
    }

    for (auto const& [posKey, overrideEntry] : placement.overrides) {
        if (overrideEntry.kind == OverrideEntry::Kind::SetBlock && overrideEntry.block != nullptr) {
            applyResolvedPos(util::decodePosKey(posKey));
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

} 
