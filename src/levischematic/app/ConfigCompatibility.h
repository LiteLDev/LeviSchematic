#pragma once

#include "Config.h"

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
            hud["panels"].push_back({{"x", -1.0}, {"y", -1.0}, {"scalePercent", 100}});
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
void applySelectionDefaults(Json& data) {
    if (!data.is_object()) return;
    if (!data.contains("selection")) data["selection"] = Json::object();
    auto& selection = data["selection"];
    if (!selection.is_object()) return;
    if (!selection.contains("faceColor")) selection["faceColor"] = Config{}.selection.faceColor;
}

} 
