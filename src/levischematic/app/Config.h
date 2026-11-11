#pragma once

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace levischematic::app {


struct Config {
    int version = 1;
    struct HudPanel {
        
        float x = -1, y = -1;
        int scalePercent = 100;
        bool operator==(HudPanel const&) const = default;
    };
    struct Hud {
        std::string position               = "bottom_left";
        std::array<HudPanel, 4> panels{};
        bool operator==(Hud const&) const = default;
    } hud;
    struct Survival {
        bool             pickBlock            = true;
        bool             easyPlace            = false;
        bool             placementRestriction = false;
        bool             holdToPlace          = true;
        bool             materialHud          = false;
        bool             verifierHud          = false;
        int              swapDelayMs          = 150;
        std::vector<int> hotbarSlots{0, 1, 2, 3, 4};
        bool             operator==(Survival const&) const = default;
    } survival;
    struct Visibility {
        int  renderDistanceBlocks                = 128;
        bool frustumCulling                      = true;
        bool operator==(Visibility const&) const = default;
    } visibility;
    
    struct Projection {
        float       alpha   = 0.15f;
        std::string texture = "textures/editor/Volume-Highlight-Overlay";
        
        bool operator==(Projection const&) const = default;
    } projection;
    struct Selection {
        std::array<float, 4> faceColor{1.0f, 1.0f, 1.0f, 0.10f};
        bool                 operator==(Selection const&) const = default;
    } selection;
    
    struct Hull {
        std::array<float, 4> blockColor{0.95f, 0.35f, 0.35f, 0.55f};
        std::array<float, 4> propertyColor{1.0f, 0.87f, 0.25f, 0.55f};
        float                expansion = 0.003f;
        float                depthBias = 100.0f;
        float                slopeBias = 15.0f;
        
        bool operator==(Hull const&) const = default;
    } hull;
    
    struct Performance {
        double   tickBudgetMs           = 2.0;
        double   frameBudgetMs          = 2.0;
        unsigned maxLoadRequests        = 4;
        unsigned geometryBuildsPerFrame = 1;
        unsigned cameraResortsPerFrame  = 8;
        unsigned queuedChunkBuilds      = 4;
    } performance;
    bool diagnostics = false;
};


inline std::string validateConfig(Config const& value) {
    if (value.version != 1) return "Unsupported configuration version.";
    if (value.survival.swapDelayMs < 0 || value.survival.swapDelayMs > 10000)
        return "Swap delay must be in [0, 10000] ms.";
    if (value.survival.hotbarSlots.empty() || value.survival.hotbarSlots.size() > 9)
        return "Choose 1 to 9 hotbar slots.";
    unsigned slots = 0;
    for (int slot : value.survival.hotbarSlots) {
        if (slot < 0 || slot > 8 || (slots & (1u << slot))) return "Hotbar slots must be unique, in [0, 8].";
        slots |= 1u << slot;
    }
    if (value.hud.position != "bottom_left" && value.hud.position != "bottom_right")
        return "HUD position must be bottom_left or bottom_right.";
    for (auto const& panel : value.hud.panels) {
        if (!std::isfinite(panel.x) || !std::isfinite(panel.y)
            || !((panel.x == -1 && panel.y == -1)
                 || (panel.x >= 0 && panel.x <= 1 && panel.y >= 0 && panel.y <= 1))
            || panel.scalePercent < 50 || panel.scalePercent > 200)
            return "HUD position must be normalized; scale must be in [50, 200]%.";
    }
    if (value.visibility.renderDistanceBlocks < 0) return "Render distance must be non-negative (0 means unlimited).";
    auto unit = [](float v) { return std::isfinite(v) && v >= 0.0f && v <= 1.0f; };
    if (!unit(value.projection.alpha) || value.projection.texture.empty()) return "Invalid projection style.";
    for (float channel : value.selection.faceColor)
        if (!unit(channel)) return "Selection RGBA channels must be finite and in [0, 1].";
    for (auto const* color : {&value.hull.blockColor, &value.hull.propertyColor})
        for (float channel : *color)
            if (!unit(channel)) return "Hull RGBA channels must be finite and in [0, 1].";
    if (!std::isfinite(value.hull.expansion) || value.hull.expansion < 0 || value.hull.expansion > 0.1f
        || !std::isfinite(value.hull.depthBias) || !std::isfinite(value.hull.slopeBias))
        return "Invalid Hull expansion or depth bias.";
    auto const& p = value.performance;
    if (!std::isfinite(p.tickBudgetMs) || !std::isfinite(p.frameBudgetMs) || p.tickBudgetMs <= 0 || p.tickBudgetMs > 50
        || p.frameBudgetMs <= 0 || p.frameBudgetMs > 50)
        return "Processing budgets must be in (0, 50] ms.";
    if (p.maxLoadRequests == 0 || p.maxLoadRequests > 64 || p.geometryBuildsPerFrame == 0
        || p.geometryBuildsPerFrame > 64 || p.cameraResortsPerFrame == 0 || p.cameraResortsPerFrame > 1024
        || p.queuedChunkBuilds == 0 || p.queuedChunkBuilds > 64)
        return "Invalid task limits.";
    return {};
}
} 
