#include "levischematic/schematic/convert/LitematicBitArray.h"
#include <bit>
#include <stdexcept>

namespace levischematic::convert {
uint32_t litematicBitsForPalette(size_t paletteSize) {
    if (paletteSize <= 1) return 2;
    auto width = static_cast<uint32_t>(std::bit_width(static_cast<uint64_t>(paletteSize - 1)));
    if (width > 32) throw std::runtime_error("Palette too large.");
    return width < 2 ? 2 : width;
}
size_t litematicLongCount(size_t count, uint32_t bits) {
    if (bits == 0 || bits > 32) throw std::runtime_error("Invalid bits per entry.");
    if (count > SIZE_MAX / bits) throw std::runtime_error("Bit array too large.");
    return (count * bits + 63) / 64;
}
std::vector<int64_t> packLitematicBits(std::span<uint32_t const> values, uint32_t bits) {
    std::vector<uint64_t> words(litematicLongCount(values.size(), bits));
    uint64_t              mask = (uint64_t{1} << bits) - 1;
    for (size_t i = 0; i < values.size(); ++i) {
        uint64_t value = values[i];
        if (value > mask) throw std::runtime_error("Palette index exceeds bit width.");
        size_t   start   = i * bits;
        size_t   word    = start >> 6;
        uint32_t offset  = static_cast<uint32_t>(start & 63);
        words[word]     |= value << offset;
        if (offset + bits > 64) words[word + 1] |= value >> (64 - offset);
    }
    std::vector<int64_t> result(words.size());
    for (size_t i = 0; i < words.size(); ++i) result[i] = static_cast<int64_t>(words[i]);
    return result;
}
std::vector<uint32_t> unpackLitematicBits(std::span<int64_t const> longs, uint32_t bits, size_t count) {
    if (longs.size() < litematicLongCount(count, bits)) throw std::runtime_error("BlockStates array is too short.");
    uint64_t              mask = (uint64_t{1} << bits) - 1;
    std::vector<uint32_t> result(count);
    for (size_t i = 0; i < count; ++i) {
        size_t   start  = i * bits;
        size_t   word   = start >> 6;
        uint32_t offset = static_cast<uint32_t>(start & 63);
        uint64_t value  = static_cast<uint64_t>(longs[word]) >> offset;
        if (offset + bits > 64) value |= static_cast<uint64_t>(longs[word + 1]) << (64 - offset);
        result[i] = static_cast<uint32_t>(value & mask);
    }
    return result;
}
}
