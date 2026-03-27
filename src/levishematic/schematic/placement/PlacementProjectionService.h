#pragma once

#include "levishematic/render/ProjectionRenderer.h"

#include <memory>

class RenderChunkCoordinator;

namespace levishematic::placement {

class PlacementRepository;

class PlacementProjectionService {
public:
    std::shared_ptr<const render::ProjectionSnapshot> rebuildProjection(const PlacementRepository& repository) const;

    void rebuildAndRefresh(
        const PlacementRepository&                     repository,
        const std::shared_ptr<RenderChunkCoordinator>& coordinator
    ) const;

    void clearProjection() const;
};

} // namespace levishematic::placement

