#include "BlockActorProjectionService.h"

namespace levischematic::app {

BlockActorProjectionService::BlockActorProjectionService(
    placement::PlacementState const&         placementState,
    verifier::BlockActorVerifierState const& verifierState,
    editor::ViewState const&                 viewState,
    render::BlockActorProjectionProjector&   projector
)
    : mPlacementState(placementState)
    , mVerifierState(verifierState)
    , mViewState(viewState)
    , mProjector(projector) {}

bool BlockActorProjectionService::flushRefresh(
    std::shared_ptr<RenderChunkCoordinator> const& coordinator
) {
    if (!mProjector.needsRefresh(mPlacementState.revision, mVerifierState.revision, mViewState.revision)) {
        return false;
    }

    mProjector.rebuildAndRefresh(mPlacementState, mVerifierState, mViewState, coordinator);
    return true;
}

std::shared_ptr<const render::BlockActorProjectionScene::DimensionScene>
BlockActorProjectionService::sceneForDimension(int dimensionId) const {
    return mProjector.sceneForDimension(dimensionId);
}

void BlockActorProjectionService::clear() {
    mProjector.clear();
}

} // namespace levischematic::app
