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

enum class HudColor { White, Green, Gold, Red, Aqua, Gray };
struct HudSegment {
    std::string text;
    HudColor    color                               = HudColor::White;
    bool        operator==(HudSegment const&) const = default;
};
using HudLine = std::vector<HudSegment>;

enum class HudToolLayout { Selection, Placement, Paste };

struct ProjectionHudSnapshot {
    HudToolLayout              layout    = HudToolLayout::Selection;
    std::string                toolKey   = "tool.selection";
    int                        toolIndex = 1, toolCount = 1;
    bool                       toolEnabled     = true;
    bool                       editTool        = false;
    bool                       deleteTool      = false;
    bool                       normalSelection = false;
    std::string                selectionName   = "Unnamed";
    std::optional<HudPosition> selectionOrigin;
    bool                       customOrigin = false;
    size_t                     regionCount  = 0;
    std::string                subRegionName;
    std::optional<HudPosition> corner1, corner2;
    bool                       expandMode = false;
    std::optional<std::string> placementName;
    size_t                     placementRegions = 0;
    bool                       regionsModified  = false;
    std::optional<HudPosition> placementOrigin;
    int                        replacePolicy = 0;
    std::string                layersKey     = "hud.layers.all";
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

inline std::string hudLineText(HudLine const& line) {
    std::string text;
    for (auto const& segment : line) text += segment.text;
    return text;
}

template <class Translate>
std::vector<HudLine> projectionHudLines(ProjectionHudSnapshot const& s, Translate const& tr) {
    std::vector<HudLine> lines;
    auto                 label = [&](std::string_view key) { return HudSegment{tr(key), HudColor::White}; };
    auto value = [](std::string text, HudColor color = HudColor::Green) { return HudSegment{std::move(text), color}; };
    auto position = [&](std::optional<HudPosition> const& p, HudColor color = HudColor::Green) {
        if (!p) return HudSegment{tr("hud.unset"), HudColor::Gray};
        return value(std::to_string((*p)[0]) + ", " + std::to_string((*p)[1]) + ", " + std::to_string((*p)[2]), color);
    };
    auto name = [&](std::string const& text) {
        auto plain = hudPlainText(text);
        return plain.empty() ? HudSegment{tr("hud.unset"), HudColor::Gray} : value(std::move(plain));
    };
    if (s.layout == HudToolLayout::Selection) {
        if (s.deleteTool) lines.push_back({label("hud.delete_target"), value(tr("hud.delete_target.area"))});
        lines.push_back(
            {label("hud.selection.prefix"),
             value(tr(s.normalSelection ? "hud.selection.normal" : "hud.selection.simple"), HudColor::Gold),
             label("hud.selection.suffix"),
             name(s.selectionName)}
        );
        lines.push_back(
            {label("hud.origin"),
             position(s.selectionOrigin),
             label("hud.bracket_open"),
             value(tr(s.customOrigin ? "hud.origin.custom" : "hud.origin.auto"), HudColor::Gold),
             label("hud.bracket_close"),
             label("hud.selection_count"),
             value(std::to_string(s.regionCount))}
        );
        lines.push_back({label("hud.subregion_name"), name(s.subRegionName)});
        auto size = hudSelectionSize(s.corner1, s.corner2);
        lines.push_back({
            label("hud.dimensions"),
            size ? value(
                       std::to_string((*size)[0]) + "x" + std::to_string((*size)[1]) + "x" + std::to_string((*size)[2])
                   )
                 : HudSegment{tr("hud.unset"), HudColor::Gray},
            label("hud.corner1"),
            position(s.corner1),
            label("hud.corner2"),
            position(s.corner2)
        });
        lines.push_back(
            {label("hud.corner_mode"), value(tr(s.expandMode ? "hud.corner_mode.expand" : "hud.corner_mode.corners"))}
        );
    } else {
        lines.push_back({
            label("hud.placement"),
            s.placementName ? name(*s.placementName) : HudSegment{tr("hud.none"), HudColor::Gray}
        });
        if (s.placementName) {
            lines.push_back(
                {label("hud.subregion_count"),
                 value(std::to_string(s.placementRegions)),
                 label("hud.subregions_modified"),
                 s.regionsModified ? value(tr("hud.yes")) : value(tr("hud.no"), HudColor::Red)}
            );
            lines.push_back({label("hud.origin"), position(s.placementOrigin)});
        }
        if (s.layout == HudToolLayout::Paste) {
            static constexpr char const* policies[] =
                {"hud.replace.target_air", "hud.replace.source_non_air", "hud.replace.all"};
            static constexpr HudColor policyColors[] = {HudColor::Red, HudColor::Gold, HudColor::Green};
            auto                      policy         = std::clamp(s.replacePolicy, 0, 2);
            lines.push_back({label("hud.replace"), value(tr(policies[policy]), policyColors[policy])});
            lines.push_back({label("hud.layers"), value(tr(s.layersKey))});
        }
    }
    lines.push_back(
        {label("hud.mode.prefix"),
         value(std::to_string(s.toolIndex)),
         label("hud.mode.separator"),
         value(std::to_string(s.toolCount)),
         label("hud.mode.suffix"),
         value(
             tr(s.toolKey),
             !s.toolEnabled ? HudColor::Red
             : s.editTool   ? HudColor::Gold
                            : HudColor::Green
         )}
    );
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
} // namespace levischematic::ui
