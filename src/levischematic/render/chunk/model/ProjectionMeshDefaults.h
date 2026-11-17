#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace levischematic::render::detail {

inline bool fillMissingTerrainMaterialIndices(
    std::span<unsigned char> bytes,
    std::size_t              vertexCount,
    std::size_t              suppliedCount,
    std::size_t              streamStart,
    std::size_t              stride,
    std::size_t              fieldOffset
) {
    if (suppliedCount >= vertexCount || stride < sizeof(std::uint16_t) || fieldOffset > stride - sizeof(std::uint16_t)
        || streamStart > bytes.size() || vertexCount > (bytes.size() - streamStart) / stride)
        return false;
    constexpr std::uint16_t noTerrainMaterial = 0xffff;
    for (auto i = suppliedCount; i < vertexCount; ++i)
        std::memcpy(
            bytes.data() + streamStart + i * stride + fieldOffset,
            &noTerrainMaterial,
            sizeof(noTerrainMaterial)
        );
    return true;
}

}
