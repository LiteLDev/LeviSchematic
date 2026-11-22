#pragma once

#include "levischematic/workspace/config/Config.h"

#include <cmath>

namespace levischematic::app {

template <class Json>
void applySurvivalDefaults(Json& data) {
    if (!data.is_object()) return;
    if (!data.contains("survival")) data["survival"] = Json::object();
    auto& s = data["survival"];
    if (!s.is_object()) return;
    auto d = Config{}.survival;
    if (!s.contains("pickBlock")) s["pickBlock"] = d.pickBlock;
    if (!s.contains("easyPlace")) s["easyPlace"] = d.easyPlace;
    if (!s.contains("placementRestriction")) s["placementRestriction"] = d.placementRestriction;
    if (!s.contains("holdToPlace")) s["holdToPlace"] = d.holdToPlace;
    if (!s.contains("materialHud")) s["materialHud"] = d.materialHud;
    if (!s.contains("verifierHud")) s["verifierHud"] = d.verifierHud;
    if (!s.contains("swapDelayMs")) s["swapDelayMs"] = d.swapDelayMs;
    if (!s.contains("hotbarSlots")) s["hotbarSlots"] = d.hotbarSlots;
}

template <class Json>
void applyHudDefaults(Json& data) {
    if (!data.is_object()) return;
    if (!data.contains("hud")) data["hud"] = Json::object();
    auto& hud = data["hud"];
    if (hud.is_object() && !hud.contains("position")) hud["position"] = Config{}.hud.position;
    if (hud.is_object() && !hud.contains("panels")) {
        hud["panels"] = Json::array();
        for (int i = 0; i < 4; ++i)
            hud["panels"].push_back({
                {"x",            -1.0},
                {"y",            -1.0},
                {"scalePercent", 100 }
            });
    }
}

template <class Json>
void applyVisibilityDefaults(Json& data) {
    if (!data.is_object()) return;
    auto const defaults = Config{}.visibility;
    if (!data.contains("visibility")) data["visibility"] = Json::object();
    auto& visibility = data["visibility"];
    if (!visibility.is_object()) return;
    if (!visibility.contains("renderDistanceBlocks"))
        visibility["renderDistanceBlocks"] = defaults.renderDistanceBlocks;
    if (!visibility.contains("frustumCulling")) visibility["frustumCulling"] = defaults.frustumCulling;
}

template <class Json>
void applyProjectionStyleDefaults(Json& data) {
    if (!data.is_object()) return;
    if (!data.contains("tool")) data["tool"] = Json::object();
    if (data["tool"].is_object() && !data["tool"].contains("item")) data["tool"]["item"] = Config{}.tool.item;
    if (!data.contains("projection")) data["projection"] = Json::object();
    if (data["projection"].is_object() && !data["projection"].contains("color"))
        data["projection"]["color"] = Config{}.projection.color;
    if (!data.contains("hull")) data["hull"] = Json::object();
    if (data["hull"].is_object()) {
        if (!data["hull"].contains("missingColor")) data["hull"]["missingColor"] = Config{}.hull.missingColor;
        if (!data["hull"].contains("alwaysShowMissing")) data["hull"]["alwaysShowMissing"] = true;
        if (!data["hull"].contains("extraColor")) data["hull"]["extraColor"] = Config{}.hull.extraColor;
        if (!data["hull"].contains("alwaysShowExtra")) data["hull"]["alwaysShowExtra"] = true;
        auto migrateColor =
            [&](char const* name, std::array<float, 4> const& former, std::array<float, 4> const& next) {
                if (!data["hull"].contains(name)) return;
                auto& color = data["hull"][name];
                if (!color.is_array() || color.size() != 4) return;
                for (std::size_t i = 0; i < 4; ++i)
                    if (!color[i].is_number() || std::abs(color[i].template get<double>() - former[i]) > 1e-6) return;
                color = next;
            };
        auto const current = Config{}.hull;
        migrateColor("missingColor", {0.2f, 0.5f, 1.0f, 0.55f}, current.missingColor);
        migrateColor("blockColor", {0.95f, 0.35f, 0.35f, 0.55f}, current.blockColor);
        migrateColor("propertyColor", {1.0f, 0.87f, 0.25f, 0.55f}, current.propertyColor);
        if (data["hull"].contains("expansion")) {
            auto& expansion = data["hull"]["expansion"];
            if (expansion.is_number() && std::abs(expansion.template get<double>() - 0.003) < 1e-6)
                expansion = Config{}.hull.expansion;
        }
    }
}

template <class Json>
void applySelectionDefaults(Json& data) {
    if (!data.is_object()) return;
    if (!data.contains("selection")) data["selection"] = Json::object();
    auto& selection = data["selection"];
    if (!selection.is_object()) return;
    if (!selection.contains("faceColor")) selection["faceColor"] = Config{}.selection.faceColor;
}

} // namespace levischematic::app
