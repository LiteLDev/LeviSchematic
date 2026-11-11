#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace levischematic::render::detail {



using HighlightMask = std::array<std::uint64_t, 2>;

constexpr bool hasComponent(HighlightMask const& mask, std::size_t index) {
    return (mask[index / 64] & (std::uint64_t{1} << (index % 64))) != 0;
}

constexpr void setComponent(HighlightMask& mask, std::size_t index, bool value) {
    auto const bit = std::uint64_t{1} << (index % 64);
    if (value) mask[index / 64] |= bit;
    else mask[index / 64] &= ~bit;
}



constexpr bool routeBlockEntityHighlight(HighlightMask& mask) {
    if (!hasComponent(mask, 12) || !hasComponent(mask, 13) || !hasComponent(mask, 25)) return false;
    bool const banner = hasComponent(mask, 5) && hasComponent(mask, 61);
    if (banner) {
        setComponent(mask, 5, false);
        setComponent(mask, 3, true);
    }
    bool const color = hasComponent(mask, 1) || hasComponent(mask, 3) || hasComponent(mask, 5);
    for (std::size_t i = 18; i <= 21; ++i) setComponent(mask, i, false);
    for (std::size_t i = 34; i <= 88; ++i) setComponent(mask, i, false);
    if (color) {
        setComponent(mask, 0, false); 
        setComponent(mask, 88, true); 
    }
    return color;
}

} 
