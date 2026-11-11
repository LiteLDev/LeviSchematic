#pragma once
#include "UiLayout.h"
#include "levischematic/app/Config.h"
#include <algorithm>
#include <cmath>

namespace levischematic::ui {
struct HudCanvas {
    UiRect safe;
    std::array<UiRect, 4> panels{};
};
inline UiRect placeHud(app::Config::HudPanel const& panel, UiRect safe, float width, float height,
                       bool right, float stackedBottom) {
    width = std::clamp(width, 0.f, safe.width);
    height = std::clamp(height, 0.f, safe.height);
    float dx = std::max(0.f, safe.width - width), dy = std::max(0.f, safe.height - height);
    float x = panel.x < 0 ? (right ? dx : 0.f) : panel.x * dx;
    float y = panel.y < 0 ? stackedBottom - height - safe.y : panel.y * dy;
    return {safe.x + std::clamp(x, 0.f, dx), safe.y + std::clamp(y, 0.f, dy), width, height};
}
inline void moveHud(app::Config::HudPanel& panel, UiRect safe, UiRect bounds, float x, float y) {
    panel.x = std::clamp((x - safe.x) / std::max(1.f, safe.width - bounds.width), 0.f, 1.f);
    panel.y = std::clamp((y - safe.y) / std::max(1.f, safe.height - bounds.height), 0.f, 1.f);
}
} 
