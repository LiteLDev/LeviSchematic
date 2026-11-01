#pragma once

#include "Config.h"

namespace levischematic::app {

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
