#include "PlacementService.h"

#include "levischematic/schematic/placement/PatchOp.h"

namespace levischematic::app {

namespace {





PlacementMutationResult successMutation(placement::PlacementInstance const* placement) {
    return PlacementMutationResult::success(placement);
}

} 

std::string PlacementError::describe() const {
    switch (code) {
    case Code::NoSelection:
        return detail.empty() ? "No placement selected." : detail;
    case Code::PlacementNotFound:
        if (!detail.empty()) {
            return detail;
        }
        return placementId
            ? "Placement ID " + std::to_string(*placementId) + " not found."
            : "Placement not found.";
    case Code::LoadFailed:
        if (loadError) {
            auto target = path.empty() ? std::string("unknown") : path.string();
            return loadError->describe(target);
        }
        return detail.empty() ? "Failed to load schematic." : detail;
    case Code::InvalidBlockOverride:
        return detail.empty() ? "Invalid block override request." : detail;
    case Code::OverrideNotFound:
        return detail.empty() ? "No block override found at the requested position." : detail;
    }

    return "Unknown placement error.";
}

PlacementService::PlacementService(
    editor::EditorState&        state,
    RuntimeContext&             runtime,
    placement::PlacementStore&  placementStore
)
    : mState(state)
    , mRuntime(runtime)
    , mPlacementStore(placementStore) {}

PlacementLoadInfo PlacementService::adoptAsset(std::shared_ptr<const placement::SchematicAsset> asset,
                                              std::filesystem::path const& path, BlockPos origin, int dimensionId) {
    PlacementLoadInfo result{.resolvedPath = path, .name = asset->defaultName};
    result.id = mPlacementStore.createPlacement(std::move(asset), origin, dimensionId, result.name, path);
    return result;
}

placement::PlacementInstance const* PlacementService::findPlacement(placement::PlacementId id) const {
    return mPlacementStore.get(id);
}

Result<placement::PlacementInstance const*, PlacementError> PlacementService::requirePlacement(
    placement::PlacementId id
) const {
    if (auto const* placement = mPlacementStore.get(id)) {
        return Result<placement::PlacementInstance const*, PlacementError>::success(placement);
    }

    return Result<placement::PlacementInstance const*, PlacementError>::failure(makeNotFoundError(id));
}

Result<placement::PlacementInstance const*, PlacementError> PlacementService::requireSelectedPlacement() const {
    if (auto const* placement = mPlacementStore.selected()) {
        return Result<placement::PlacementInstance const*, PlacementError>::success(placement);
    }

    return Result<placement::PlacementInstance const*, PlacementError>::failure({
        .code   = PlacementError::Code::NoSelection,
        .detail = "No placement selected. Use /schem load or /schem select first.",
    });
}

std::optional<placement::PlacementId> PlacementService::selectedPlacementId() const {
    return mState.placements.selectedId;
}

std::vector<std::reference_wrapper<placement::PlacementInstance const>> PlacementService::orderedPlacements() const {
    std::vector<std::reference_wrapper<placement::PlacementInstance const>> placements;
    placements.reserve(mState.placements.order.size());

    for (auto placementId : mState.placements.order) {
        if (auto const* placement = mPlacementStore.get(placementId)) {
            placements.emplace_back(*placement);
        }
    }

    return placements;
}

Result<RemovedPlacementInfo, PlacementError> PlacementService::removePlacement(placement::PlacementId id) {
    auto const* placement = mPlacementStore.get(id);
    if (!placement) {
        return Result<RemovedPlacementInfo, PlacementError>::failure(makeNotFoundError(id));
    }

    RemovedPlacementInfo removed{placement->id, placement->name};
    (void)mPlacementStore.remove(id);
    return Result<RemovedPlacementInfo, PlacementError>::success(std::move(removed));
}

void PlacementService::clearPlacements() {
    mPlacementStore.clear();
}

PlacementStatus PlacementService::selectPlacement(placement::PlacementId id) {
    if (!mPlacementStore.select(id)) {
        return PlacementStatus::failure(makeNotFoundError(id));
    }

    return ok<PlacementError>();
}

PlacementMutationResult PlacementService::togglePlacementEnabled(placement::PlacementId id) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    (void)mPlacementStore.toggleEnabled(id);
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::togglePlacementRender(placement::PlacementId id) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    (void)mPlacementStore.toggleRender(id);
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::movePlacement(placement::PlacementId id, int dx, int dy, int dz) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    (void)mPlacementStore.move(id, dx, dy, dz);
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::setPlacementOrigin(placement::PlacementId id, BlockPos origin) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    (void)mPlacementStore.setOrigin(id, origin);
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::rotatePlacement(
    placement::PlacementId                 id,
    placement::PlacementInstance::Rotation delta
) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    (void)mPlacementStore.rotate(id, delta);
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::setPlacementMirror(
    placement::PlacementId               id,
    placement::PlacementInstance::Mirror mirror
) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    (void)mPlacementStore.setMirror(id, mirror);
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::resetPlacementTransform(placement::PlacementId id) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    (void)mPlacementStore.resetTransform(id);
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::patchPlacementBlock(
    placement::PlacementId id,
    BlockPos               worldPos,
    placement::PatchOp const& op
) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    if (op.kind == placement::PatchOp::Kind::SetBlock && op.block == nullptr) {
        return PlacementMutationResult::failure({
            .code        = PlacementError::Code::InvalidBlockOverride,
            .placementId = id,
            .detail      = "Cannot set an override without a block definition.",
        });
    }

    if (!(mPlacementStore.patchBlock(id, worldPos, op))) {
        return PlacementMutationResult::failure({
            .code        = op.kind == placement::PatchOp::Kind::ClearOverride
                ? PlacementError::Code::OverrideNotFound
                : PlacementError::Code::InvalidBlockOverride,
            .placementId = id,
            .detail      = op.kind == placement::PatchOp::Kind::ClearOverride
                ? "No block override found at the requested position."
                : "Failed to apply the block override.",
        });
    }

    return successMutation(mPlacementStore.get(id));
}

PlacementError PlacementService::makeNotFoundError(
    std::optional<placement::PlacementId> id,
    std::string                           detail
) const {
    return PlacementError{
        .code        = PlacementError::Code::PlacementNotFound,
        .placementId = id,
        .detail      = std::move(detail),
    };
}

} 
