#pragma once
#include <array>
#include <cstdint>
#include <optional>
namespace levischematic::placement {

inline std::optional<std::array<int, 3>> inverseProjectionPosition(
    std::array<int, 3> world,
    std::array<int, 3> origin,
    std::array<int, 3> size,
    int                mirror,
    int                rotation
) {
    int64_t x = int64_t(world[0]) - origin[0], y = int64_t(world[1]) - origin[1], z = int64_t(world[2]) - origin[2];
    int64_t lx = x, lz = z;
    switch (rotation) {
    case 1:
        lx = z;
        lz = int64_t(size[2]) - 1 - x;
        break;
    case 2:
        lx = int64_t(size[0]) - 1 - x;
        lz = int64_t(size[2]) - 1 - z;
        break;
    case 3:
        lx = int64_t(size[0]) - 1 - z;
        lz = x;
        break;
    default:
        break;
    }
    if (mirror == 1) lx = int64_t(size[0]) - 1 - lx;
    if (mirror == 2) lz = int64_t(size[2]) - 1 - lz;
    if (lx < 0 || y < 0 || lz < 0 || lx >= size[0] || y >= size[1] || lz >= size[2]) return {};
    return std::array<int, 3>{static_cast<int>(lx), static_cast<int>(y), static_cast<int>(lz)};
}
} 
