#pragma once
#include "levischematic/editor/LayerRange.h"
#include <array>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>

namespace levischematic::session {
using Json                           = nlohmann::ordered_json;
inline constexpr int DocumentVersion = 2;
inline int           decodeInteger(Json const& value) {
    if (!value.is_number_integer()) throw std::runtime_error("Saved value is not an integer.");
    if (value.is_number_unsigned()) {
        auto number = value.get<uint64_t>();
        if (number > INT32_MAX) throw std::runtime_error("Saved integer overflow.");
        return static_cast<int>(number);
    }
    auto number = value.get<int64_t>();
    if (number < INT32_MIN || number > INT32_MAX) throw std::runtime_error("Saved integer overflow.");
    return static_cast<int>(number);
}
inline std::string identityFileKey(std::string const& identity) {
    
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : identity) hash = (hash ^ c) * 1099511628211ull;
    char           buffer[17]{};
    constexpr char hex[] = "0123456789abcdef";
    for (int i = 15; i >= 0; --i) {
        buffer[i]   = hex[hash & 15];
        hash      >>= 4;
    }
    return buffer;
}
inline void validateDocument(Json const& doc, std::string const& identity) {
    auto version=decodeInteger(doc.at("version"));
    if (version<1 || version>DocumentVersion) throw std::runtime_error("Unsupported session version.");
    if (doc.at("identity").get<std::string>() != identity) throw std::runtime_error("Session world identity differs.");
    if (!doc.at("dimensions").is_object()) throw std::runtime_error("Invalid session dimensions.");
}
inline Json encodeLayer(editor::LayerRange const& range) {
    return {
        {"axis",         static_cast<int>(range.axis)},
        {"mode",         static_cast<int>(range.mode)},
        {"layer",        range.layer                 },
        {"above",        range.above                 },
        {"below",        range.below                 },
        {"minimum",      range.minY                  },
        {"maximum",      range.maxY                  },
        {"follow",       range.followPlayer          },
        {"move_minimum", range.moveMinimum           },
        {"move_maximum", range.moveMaximum           }
    };
}
inline editor::LayerRange decodeLayer(Json const& value) {
    editor::LayerRange range;
    int                axis = decodeInteger(value.at("axis")), mode = decodeInteger(value.at("mode"));
    if (axis < 0 || axis > 2 || mode < 0 || mode > 4) throw std::runtime_error("Invalid saved layer mode.");
    range.axis         = static_cast<editor::LayerAxis>(axis);
    range.mode         = static_cast<editor::LayerMode>(mode);
    range.enabled      = range.mode != editor::LayerMode::All;
    range.layer        = decodeInteger(value.at("layer"));
    range.minY         = decodeInteger(value.at("minimum"));
    range.above        = decodeInteger(value.at("above"));
    range.below        = decodeInteger(value.at("below"));
    range.moveMinimum  = value.at("move_minimum").get<bool>();
    range.moveMaximum  = value.at("move_maximum").get<bool>();
    range.maxY         = decodeInteger(value.at("maximum"));
    range.followPlayer = value.at("follow").get<bool>();
    if (range.minY > range.maxY) throw std::runtime_error("Invalid saved layer bounds.");
    return range;
}
inline std::array<int, 3> decodePosition(Json const& value) {
    if (!value.is_array() || value.size() != 3) throw std::runtime_error("Invalid saved coordinate.");
    std::array<int, 3> result{};
    for (size_t i = 0; i < 3; ++i) {
        result[i] = decodeInteger(value[i]);
    }
    return result;
}
} 
