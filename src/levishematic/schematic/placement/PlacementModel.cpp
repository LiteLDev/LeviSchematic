#include "PlacementModel.h"

#include "levishematic/util/PositionUtils.h"

#include <algorithm>

namespace levishematic::placement {

int PlacementInstance::totalNonAirBlocks() const noexcept {
    return asset ? asset->totalNonAirBlocks() : 0;
}

std::string PlacementInstance::describeTransform() const {
    static constexpr const char* kRotationNames[] = {"0deg", "CW90", "180deg", "CCW90"};
    static constexpr const char* kMirrorNames[]   = {"None", "X", "Z"};

    return "Origin:(" + std::to_string(origin.x) + "," + std::to_string(origin.y) + ","
         + std::to_string(origin.z) + ") Rot:" + kRotationNames[static_cast<int>(rotation)]
         + " Mir:" + kMirrorNames[static_cast<int>(mirror)];
}

PlacementInstance::Rotation rotateBy(
    PlacementInstance::Rotation base,
    PlacementInstance::Rotation delta
) {
    return static_cast<PlacementInstance::Rotation>(
        (static_cast<int>(base) + static_cast<int>(delta)) % 4
    );
}

BlockPos transformLocalPos(
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
        if (!entry.block || entry.block->isAir()) {
            continue;
        }

        auto local = transformLocalPos(
            entry.localPos,
            placement.asset->size,
            placement.mirror,
            placement.rotation
        );
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

    auto it = resolvedPositions.begin();
    auto minPos = it->second;
    auto maxPos = it->second;
    ++it;

    for (; it != resolvedPositions.end(); ++it) {
        auto const& worldPos = it->second;
        minPos.x = std::min(minPos.x, worldPos.x);
        minPos.y = std::min(minPos.y, worldPos.y);
        minPos.z = std::min(minPos.z, worldPos.z);
        maxPos.x = std::max(maxPos.x, worldPos.x);
        maxPos.y = std::max(maxPos.y, worldPos.y);
        maxPos.z = std::max(maxPos.z, worldPos.z);
    }

    return {minPos, maxPos};
}

} // namespace levishematic::placement
