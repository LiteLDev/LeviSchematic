#include "ProjectionService.h"

namespace levishematic::app {

ProjectionService::ProjectionService(
    placement::PlacementState const& placementState,
    render::ProjectionProjector&     projector
)
    : mPlacementState(placementState)
    , mProjector(projector) {}

bool ProjectionService::flushRefresh(
    std::shared_ptr<RenderChunkCoordinator> const& coordinator
) {
    if (!mProjector.needsRefresh(mPlacementState.revision)) {
        return false;
    }

    mProjector.rebuildAndRefresh(mPlacementState, coordinator);
    return true;
}

std::shared_ptr<const render::ProjectionScene> ProjectionService::scene() const {
    return mProjector.scene();
}

void ProjectionService::clear() {
    mProjector.clear();
}

} // namespace levishematic::app
