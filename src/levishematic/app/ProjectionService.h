#pragma once

#include "levishematic/render/ProjectionRenderer.h"
#include "levishematic/schematic/placement/PlacementStore.h"

#include <memory>

class RenderChunkCoordinator;

namespace levishematic::app {

class ProjectionService {
public:
    ProjectionService(
        placement::PlacementState const& placementState,
        render::ProjectionProjector&     projector
    );

    [[nodiscard]] bool flushRefresh(
        std::shared_ptr<RenderChunkCoordinator> const& coordinator
    );
    [[nodiscard]] std::shared_ptr<const render::ProjectionScene> scene() const;
    void                                                         clear();

private:
    placement::PlacementState const& mPlacementState;
    render::ProjectionProjector&     mProjector;
};

} // namespace levishematic::app
