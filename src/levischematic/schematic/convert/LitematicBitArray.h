#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace levischematic::convert {
uint32_t              litematicBitsForPalette(size_t paletteSize);
size_t                litematicLongCount(size_t count, uint32_t bits);
std::vector<int64_t>  packLitematicBits(std::span<uint32_t const> values, uint32_t bits);
std::vector<uint32_t> unpackLitematicBits(std::span<int64_t const> longs, uint32_t bits, size_t count);
} // namespace levischematic::convert
