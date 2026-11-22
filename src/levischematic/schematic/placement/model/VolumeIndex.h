#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace levischematic::placement {
inline std::optional<size_t> volumeSize(std::array<int, 3> size) {
    uint64_t result = 1;
    for (int value : size) {
        if (value <= 0 || result > uint64_t{INT32_MAX} / static_cast<unsigned>(value)) return {};
        result *= static_cast<unsigned>(value);
    }
    return static_cast<size_t>(result);
}
inline size_t volumeIndex(std::array<int, 3> pos, std::array<int, 3> size) {
    return (static_cast<size_t>(pos[0]) * size[1] + pos[1]) * size[2] + pos[2];
}
inline std::array<int, 3> volumePosition(size_t index, std::array<int, 3> size) {
    return {
        static_cast<int>(index / (size_t(size[1]) * size[2])),
        static_cast<int>((index / size[2]) % size[1]),
        static_cast<int>(index % size[2])
    };
}
template <class Indices>
inline std::optional<size_t> secondaryLayerIndex(Indices const* extra, size_t index, bool edited) {
    if (extra == nullptr || edited || index >= extra->size()) return {};
    auto value = (*extra)[index];
    if (value < 0) return {};
    return static_cast<size_t>(value);
}
} // namespace levischematic::placement
