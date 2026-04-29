#include "ProjectionService.h"

namespace levischematic::app {

ProjectionService::ProjectionService(
    placement::PlacementState const& placementState,
    verifier::VerifierState const&   verifierState,
    editor::ViewState const&         viewState,
    render::ProjectionProjector&     projector
)
    : mPlacementState(placementState)
    , mVerifierState(verifierState)
    , mViewState(viewState)
    , mProjector(projector) {}

bool ProjectionService::flushRefresh(
    std::shared_ptr<RenderChunkCoordinator> const& coordinator
) {
    if (!mProjector.needsRefresh(mPlacementState.revision, mVerifierState.revision, mViewState.revision)) {
        return false;
    }

    mProjector.rebuildAndRefresh(mPlacementState, mVerifierState, mViewState, coordinator);
    return true;
}

std::shared_ptr<const render::ProjectionScene::DimensionScene> ProjectionService::sceneForDimension(int dimensionId) const {
    return mProjector.sceneForDimension(dimensionId);
}

void ProjectionService::clear() {
    mProjector.clear();
}

} // namespace levischematic::app
