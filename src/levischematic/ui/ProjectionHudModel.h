#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace levischematic::ui {

using HudPosition = std::array<int64_t, 3>;


struct ProjectionHudSnapshot {
    bool                       selectionMode = true;
    std::string                fileName;
    std::optional<HudPosition> corner1, corner2, origin, size;
    int                        rotation = 0;
    int                        mirror   = 0;
    std::string                toolDetail;
};

inline std::optional<HudPosition>
hudSelectionSize(std::optional<HudPosition> const& first, std::optional<HudPosition> const& second) {
    if (!first || !second) return {};
    HudPosition result;
    for (size_t i = 0; i < 3; ++i)
        result[i] = std::max((*first)[i], (*second)[i]) - std::min((*first)[i], (*second)[i]) + 1;
    return result;
}


inline std::string hudPlainText(std::string_view value) {
    std::string result;
    for (size_t i = 0; i < value.size(); ++i) {
        auto c = static_cast<unsigned char>(value[i]);
        if (c == 0xc2 && i + 1 < value.size() && static_cast<unsigned char>(value[i + 1]) == 0xa7) {
            ++i;
            if (i + 1 < value.size()) {
                ++i;
                while (i + 1 < value.size() && (static_cast<unsigned char>(value[i + 1]) & 0xc0) == 0x80) ++i;
            }
        } else if (c >= 0x20 && c != 0x7f) result += value[i];
    }
    return result;
}

template <class Translate>
std::vector<std::string> projectionHudLines(ProjectionHudSnapshot const& snapshot, Translate const& tr) {
    std::vector<std::string> lines;
    auto add = [&](std::string_view key, std::string value) { lines.push_back(tr(key) + ": " + std::move(value)); };
    auto position = [&](std::optional<HudPosition> const& value, std::string_view separator) {
        if (!value) return tr("hud.unset");
        return std::to_string((*value)[0]) + std::string(separator) + std::to_string((*value)[1])
             + std::string(separator) + std::to_string((*value)[2]);
    };
    add("hud.mode", tr(snapshot.selectionMode ? "hud.selection" : "hud.projection") + snapshot.toolDetail);
    add("hud.file", snapshot.fileName.empty() ? tr("hud.unloaded") : hudPlainText(snapshot.fileName));
    if (snapshot.selectionMode) {
        add("hud.corner1", position(snapshot.corner1, ", "));
        add("hud.corner2", position(snapshot.corner2, ", "));
        add("hud.size", position(hudSelectionSize(snapshot.corner1, snapshot.corner2), " × "));
    } else {
        add("hud.origin", position(snapshot.origin, ", "));
        add("hud.size", position(snapshot.size, " × "));
        add("hud.rotation", snapshot.origin ? std::to_string(snapshot.rotation * 90) + "°" : tr("hud.unset"));
        add("hud.mirror",
            !snapshot.origin       ? tr("hud.unset")
            : snapshot.mirror == 1 ? "X"
            : snapshot.mirror == 2 ? "Z"
                                   : tr("ui.mirror.none"));
    }
    return lines;
}


template <class Measure>
std::string fitHudText(std::string text, float width, Measure const& measure) {
    if (measure(text) <= width) return text;
    std::string const ellipsis = "…";
    if (measure(ellipsis) > width) return {};
    while (!text.empty()) {
        size_t end = text.size() - 1;
        while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
        text.resize(end);
        if (measure(text + ellipsis) <= width) return text + ellipsis;
    }
    return ellipsis;
}

struct HudLayout {
    float x, y, width, height;
};


inline std::optional<HudLayout> projectionHudLayout(
    float  left,
    float  top,
    float  right,
    float  bottom,
    float  textWidth,
    float  lineHeight,
    size_t lines,
    bool   onRight
) {
    float availableWidth = right - left - 16;
    float height         = lineHeight * static_cast<float>(lines) + 8;
    float width          = std::min(textWidth + 8, availableWidth * 0.45f);
    if (!std::isfinite(width) || !std::isfinite(height) || width < 16 || lineHeight <= 0 || lines == 0
        || bottom - top < height + 56)
        return {};
    return HudLayout{onRight ? right - 8 - width : left + 8, bottom - 48 - height, width, height};
}
} 
