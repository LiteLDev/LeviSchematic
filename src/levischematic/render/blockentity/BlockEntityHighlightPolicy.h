#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace levischematic::render::detail {


using HighlightMask = std::array<std::uint64_t, 2>;

inline constexpr std::uint16_t projectionCoverageView   = 0xffff;
inline constexpr std::uint16_t projectionColorView      = 0xfffe;
inline constexpr std::uint16_t validationFaceView       = 0xfffd;
inline constexpr std::uint16_t validationEdgeView       = 0xfffc;
inline constexpr std::uint16_t projectionBlendDepthView = 0xfffb;

inline std::vector<std::uint16_t> projectionPassViews(std::span<std::uint16_t const> original, bool coverage) {
    if (coverage) return {projectionCoverageView};
    std::vector<std::uint16_t> result;
    result.reserve(original.size() + 1);
    for (auto id : original)
        if (id != projectionCoverageView && id != projectionBlendDepthView) result.push_back(id);
    if (result.empty()) result.push_back(0);
    result.push_back(projectionColorView);
    return result;
}

inline std::vector<std::uint16_t> projectionColorPassViews(std::span<std::uint16_t const> original) {
    std::vector<std::uint16_t> result;
    result.reserve(original.size() + 2);
    for (auto id : original)
        if (id != projectionBlendDepthView) result.push_back(id);
    if (result.empty()) result = {0, projectionColorView};
    return result;
}

constexpr bool hasComponent(HighlightMask const& mask, std::size_t index) {
    return (mask[index / 64] & (std::uint64_t{1} << (index % 64))) != 0;
}

constexpr void setComponent(HighlightMask& mask, std::size_t index, bool value) {
    auto const bit = std::uint64_t{1} << (index % 64);
    if (value) mask[index / 64] |= bit;
    else mask[index / 64] &= ~bit;
}

constexpr void restoreWorldItemPass(HighlightMask& mask, bool blending, bool alphaTest) {
    setComponent(mask, 3, !blending && !alphaTest);
    setComponent(mask, 1, !blending && alphaTest);
    setComponent(mask, 5, blending);
}


constexpr bool routeBlockEntityHighlight(HighlightMask& mask, bool preserveView = false) {
    if (!hasComponent(mask, 12) || !hasComponent(mask, 13) || !hasComponent(mask, 25)) return false;
    bool const banner = hasComponent(mask, 5) && hasComponent(mask, 61);
    if (banner) {
        setComponent(mask, 5, false);
        setComponent(mask, 1, true);
    }
    bool const color = hasComponent(mask, 1) || hasComponent(mask, 3) || hasComponent(mask, 5);
    for (std::size_t i = 18; i <= 21; ++i) setComponent(mask, i, false);
    for (std::size_t i = 34; i <= 88; ++i) setComponent(mask, i, false);
    if (color) {
        if (!preserveView) setComponent(mask, 0, false);
        setComponent(mask, 88, true);
    }
    return color;
}

inline constexpr std::size_t selectionOverlayComponent = 70;

constexpr bool routeValidationMarker(HighlightMask& mask) {
    if (!hasComponent(mask, 0) || !hasComponent(mask, 12) || !hasComponent(mask, 13)) return false;
    setComponent(mask, 1, false);
    setComponent(mask, 3, false);
    setComponent(mask, 5, true);
    for (std::size_t i = 34; i < 92; ++i) setComponent(mask, i, false);
    setComponent(mask, selectionOverlayComponent, true);
    return true;
}

constexpr void disableRecordPasses(HighlightMask& mask) {
    for (std::size_t i = 1; i <= 6; ++i) setComponent(mask, i, false);
}

}
