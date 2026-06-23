#pragma once

#include "levischematic/editor/EditorState.h"
#include "levischematic/render/BlockActorProjectionRenderer.h"
#include "levischematic/schematic/placement/PlacementStore.h"
#include "levischematic/verifier/VerifierTypes.h"

#include <memory>

class RenderChunkCoordinator;

namespace levischematic::app {

class BlockActorProjectionService {
public:
    BlockActorProjectionService(
        placement::PlacementState const&          placementState,
        verifier::BlockActorVerifierState const&  verifierState,
        editor::ViewState const&                  viewState,
        render::BlockActorProjectionProjector&    projector
    );

    [[nodiscard]] bool flushRefresh(std::shared_ptr<RenderChunkCoordinator> const& coordinator);
    [[nodiscard]] std::shared_ptr<const render::BlockActorProjectionScene::DimensionScene> sceneForDimension(
        int dimensionId
    ) const;
    void clear();

private:
    placement::PlacementState const&         mPlacementState;
    verifier::BlockActorVerifierState const& mVerifierState;
    editor::ViewState const&                 mViewState;
    render::BlockActorProjectionProjector&   mProjector;
};

} // namespace levischematic::app
