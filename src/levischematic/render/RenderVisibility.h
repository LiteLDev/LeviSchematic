#pragma once

#include "SchematicRenderAlgorithms.h"
#include "levischematic/app/Config.h"
#include "mc/client/renderer/game/LevelRendererCamera.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/phys/AABB.h"

namespace levischematic::render {

enum class VisibilityResult { Visible, Distance, Frustum };

struct VisibilityStats {
    std::size_t candidates     = 0;
    std::size_t distanceCulled = 0;
    std::size_t frustumCulled  = 0;
    void        record(VisibilityResult result) {
        ++candidates;
        distanceCulled += result == VisibilityResult::Distance;
        frustumCulled  += result == VisibilityResult::Frustum;
    }
};

inline AABB subChunkBounds(Vec3 const& origin, float expansion = 0.01f) {
    return AABB{
        {        origin.x - expansion,         origin.y - expansion,         origin.z - expansion},
        {origin.x + 16.0f + expansion, origin.y + 16.0f + expansion, origin.z + 16.0f + expansion}
    };
}

inline VisibilityResult testVisibility(
    Vec3 const&                    camera,
    AABB const&                    bounds,
    app::Config::Visibility const& config,
    LevelRendererCamera const*     renderer,
    bool                           useFrustum = true
) {
    if (!detail::validArea({bounds.min.x, bounds.min.y, bounds.min.z}, {bounds.max.x, bounds.max.y, bounds.max.z})
        || !std::isfinite(camera.x) || !std::isfinite(camera.y) || !std::isfinite(camera.z))
        return VisibilityResult::Visible;
    if (!detail::withinRenderDistance(
            {camera.x, camera.y, camera.z},
            {bounds.min.x, bounds.min.y, bounds.min.z},
            {bounds.max.x, bounds.max.y, bounds.max.z},
            config.renderDistanceBlocks
        ))
        return VisibilityResult::Distance;
    if (renderer && useFrustum && config.frustumCulling) {
        Vec3 center{
            (bounds.min.x + bounds.max.x) * 0.5f,
            (bounds.min.y + bounds.max.y) * 0.5f,
            (bounds.min.z + bounds.max.z) * 0.5f
        };
        double radiusSquared = 0;
        for (double extent :
             {static_cast<double>(bounds.max.x) - bounds.min.x,
              static_cast<double>(bounds.max.y) - bounds.min.y,
              static_cast<double>(bounds.max.z) - bounds.min.z})
            radiusSquared += extent * extent;
        float radius = static_cast<float>(std::sqrt(radiusSquared) * 0.5 + 0.01);
        if (std::isfinite(center.x) && std::isfinite(center.y) && std::isfinite(center.z) && std::isfinite(radius)
            && !renderer->cullerIsVisible(center, radius))
            return VisibilityResult::Frustum;
    }
    return VisibilityResult::Visible;
}

} 
