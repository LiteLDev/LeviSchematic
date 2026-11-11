#pragma once
#include "SurvivalAlgorithms.h"
#include <map>

namespace levischematic::survival {

inline bool
placementOrientationMatches(std::map<std::string, int> const& expected, std::map<std::string, int> const& predicted) {
    for (auto const* name :
         {"direction",
          "minecraft:cardinal_direction",
          "facing_direction",
          "pillar_axis",
          "ground_sign_direction",
          "torch_facing_direction",
          "upside_down_bit",
          "top_slot_bit",
          "door_hinge_bit",
          "attachment",
          "lever_direction",
          "hanging",
          "vertical_half"}) {
        auto a = expected.find(name), b = predicted.find(name);
        if ((a == expected.end()) != (b == predicted.end())) return false;
        if (a != expected.end() && a->second != b->second) return false;
    }
    return true;
}
struct BuildRule {
    unsigned count     = 1;
    bool     secondary = false;
    Position partnerOffset{};
};

inline BuildRule buildRule(std::string_view name, std::map<std::string, int> const& states) {
    auto value = [&](char const* key, int fallback = 0) {
        auto it = states.find(key);
        return it == states.end() ? fallback : it->second;
    };
    BuildRule result;
    if (states.contains("upper_block_bit")) {
        result.secondary     = value("upper_block_bit") != 0;
        result.partnerOffset = {0, result.secondary ? -1 : 1, 0};
    }
    if (states.contains("head_piece_bit")) {
        result.secondary             = value("head_piece_bit") != 0;
        int                direction = value("direction", value("minecraft:cardinal_direction", -1));
        constexpr Position steps[]   = {
            {0,  0, 1 },
            {-1, 0, 0 },
            {0,  0, -1},
            {1,  0, 0 }
        };
        if (direction >= 0 && direction < 4) {
            result.partnerOffset = steps[direction];
            if (result.secondary)
                for (auto& axis : result.partnerOffset) axis = -axis;
        }
    }
    if (name.find("double_") != std::string_view::npos && name.find("slab") != std::string_view::npos) result.count = 2;
    if (name == "minecraft:snow_layer") result.count = static_cast<unsigned>(std::clamp(value("height") + 1, 1, 8));
    if (name == "minecraft:turtle_egg")
        result.count = static_cast<unsigned>(std::clamp(value("turtle_egg_count") + 1, 1, 4));
    if (name == "minecraft:sea_pickle")
        result.count = static_cast<unsigned>(std::clamp(value("cluster_count") + 1, 1, 4));
    if (name.ends_with("candle") && !name.ends_with("candle_cake"))
        result.count = static_cast<unsigned>(std::clamp(value("candles") + 1, 1, 4));
    return result;
}
} 
