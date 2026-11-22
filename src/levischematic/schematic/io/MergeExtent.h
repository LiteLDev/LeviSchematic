#pragma once
#include "levischematic/schematic/placement/model/VolumeIndex.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace levischematic::schematic {
inline std::optional<std::array<int, 3>>
mergedExtent(std::span<std::array<int, 3> const> offsets, std::span<std::array<int, 3> const> sizes) {
    if (offsets.size() != sizes.size() || offsets.empty()) return {};
    std::array<int64_t, 3> extent{};
    for (size_t i = 0; i < offsets.size(); ++i)
        for (int axis = 0; axis < 3; ++axis) {
            if (offsets[i][axis] < 0 || sizes[i][axis] <= 0) return {};
            extent[axis] = std::max(extent[axis], int64_t{offsets[i][axis]} + sizes[i][axis]);
        }
    for (auto value : extent)
        if (value > INT32_MAX) return {};
    std::array<int, 3> result{static_cast<int>(extent[0]), static_cast<int>(extent[1]), static_cast<int>(extent[2])};
    if (!placement::volumeSize(result)) return {};
    return result;
}
} // namespace levischematic::schematic
