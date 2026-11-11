#pragma once

#include "PlacementModel.h"
#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace levischematic::placement {

inline std::optional<BlockPos> checkedPosition(std::array<int64_t, 3> value) {
    for (auto n : value)
        if (n < INT32_MIN || n > INT32_MAX) return {};
    return BlockPos{static_cast<int>(value[0]), static_cast<int>(value[1]), static_cast<int>(value[2])};
}
inline std::optional<BlockPos> toLocal(PlacementInstance const& p, BlockPos world) {
    if (!p.asset) return {};
    int64_t x = int64_t(world.x) - p.origin.x, y = int64_t(world.y) - p.origin.y, z = int64_t(world.z) - p.origin.z;
    auto    size = p.asset->size;
    switch (p.rotation) {
    case PlacementInstance::Rotation::CW_90: {
        auto old = x;
        x        = z;
        z        = int64_t(size.z) - 1 - old;
        break;
    }
    case PlacementInstance::Rotation::CW_180:
        x = int64_t(size.x) - 1 - x;
        z = int64_t(size.z) - 1 - z;
        break;
    case PlacementInstance::Rotation::CCW_90: {
        auto old = x;
        x        = int64_t(size.x) - 1 - z;
        z        = old;
        break;
    }
    default:
        break;
    }
    if (p.mirror == PlacementInstance::Mirror::MIRROR_X) x = int64_t(size.x) - 1 - x;
    if (p.mirror == PlacementInstance::Mirror::MIRROR_Z) z = int64_t(size.z) - 1 - z;
    return checkedPosition({x, y, z});
}
inline std::optional<BlockPos> toWorld(PlacementInstance const& p, BlockPos local) {
    if (!p.asset) return {};
    int64_t x = local.x, y = local.y, z = local.z;
    auto    size = p.asset->size;
    if (p.mirror == PlacementInstance::Mirror::MIRROR_X) x = int64_t(size.x) - 1 - x;
    if (p.mirror == PlacementInstance::Mirror::MIRROR_Z) z = int64_t(size.z) - 1 - z;
    switch (p.rotation) {
    case PlacementInstance::Rotation::CW_90: {
        auto old = x;
        x        = int64_t(size.z) - 1 - z;
        z        = old;
        break;
    }
    case PlacementInstance::Rotation::CW_180:
        x = int64_t(size.x) - 1 - x;
        z = int64_t(size.z) - 1 - z;
        break;
    case PlacementInstance::Rotation::CCW_90: {
        auto old = x;
        x        = z;
        z        = int64_t(size.x) - 1 - old;
        break;
    }
    default:
        break;
    }
    return checkedPosition({x + p.origin.x, y + p.origin.y, z + p.origin.z});
}


inline std::array<double, 3> transformPoint(PlacementInstance const& p, std::array<double, 3> point) {
    auto [x, y, z] = point;
    auto size      = p.asset->size;
    if (p.mirror == PlacementInstance::Mirror::MIRROR_X) x = size.x - x;
    if (p.mirror == PlacementInstance::Mirror::MIRROR_Z) z = size.z - z;
    switch (p.rotation) {
    case PlacementInstance::Rotation::CW_90: {
        auto old = x;
        x        = size.z - z;
        z        = old;
        break;
    }
    case PlacementInstance::Rotation::CW_180:
        x = size.x - x;
        z = size.z - z;
        break;
    case PlacementInstance::Rotation::CCW_90: {
        auto old = x;
        x        = z;
        z        = size.x - old;
        break;
    }
    default:
        break;
    }
    return {x + p.origin.x, y + p.origin.y, z + p.origin.z};
}
inline float transformYaw(PlacementInstance const& p, float yaw) {
    if (p.mirror == PlacementInstance::Mirror::MIRROR_X) yaw = -yaw;
    if (p.mirror == PlacementInstance::Mirror::MIRROR_Z) yaw = 180 - yaw;
    return std::remainder(yaw + 90 * static_cast<int>(p.rotation), 360.0f);
}
} 
