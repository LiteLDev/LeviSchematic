#include "PlacementService.h"

#include "levishematic/render/ProjectionRenderer.h"

namespace levishematic::app {

namespace {

PlacementMutationResult successMutation(placement::PlacementInstance const* placement) {
    return PlacementMutationResult::success(placement);
}

} // namespace

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
    placement::PlacementStore&  placementStore,
    placement::SchematicLoader& schematicLoader
)
    : mState(state)
    , mRuntime(runtime)
    , mPlacementStore(placementStore)
    , mSchematicLoader(schematicLoader) {}

Result<PlacementLoadInfo, PlacementError> PlacementService::loadSchematic(
    std::string const& filename,
    BlockPos           origin,
    std::string const& explicitName
) {
    auto resolvedPath = mRuntime.resolveSchematicPath(filename);
    auto assetResult  = mSchematicLoader.loadMcstructureAsset(resolvedPath);
    if (!assetResult) {
        return Result<PlacementLoadInfo, PlacementError>::failure({
            .code      = PlacementError::Code::LoadFailed,
            .path      = resolvedPath,
            .loadError = assetResult.error(),
        });
    }

    auto displayName = explicitName.empty() ? assetResult.value()->defaultName : explicitName;
    auto placementId = mPlacementStore.createPlacement(
        assetResult.value(),
        origin,
        displayName,
        resolvedPath
    );

    auto const* placement = mPlacementStore.get(placementId);
    return Result<PlacementLoadInfo, PlacementError>::success({
        .id           = placementId,
        .resolvedPath = resolvedPath,
        .name         = placement ? placement->name : displayName,
    });
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
    mPlacementStore.remove(id);
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

    mPlacementStore.toggleEnabled(id);
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::togglePlacementRender(placement::PlacementId id) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    mPlacementStore.toggleRender(id);
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::movePlacement(placement::PlacementId id, int dx, int dy, int dz) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    mPlacementStore.move(id, dx, dy, dz);
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::setPlacementOrigin(placement::PlacementId id, BlockPos origin) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    mPlacementStore.setOrigin(id, origin);
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

    mPlacementStore.rotate(id, delta);
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

    mPlacementStore.setMirror(id, mirror);
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::resetPlacementTransform(placement::PlacementId id) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    mPlacementStore.resetTransform(id);
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::patchPlacementBlock(
    placement::PlacementId id,
    BlockPos               worldPos,
    render::PatchOp const& op
) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    if (op.kind == render::PatchOp::Kind::SetBlock && op.block == nullptr) {
        return PlacementMutationResult::failure({
            .code        = PlacementError::Code::InvalidBlockOverride,
            .placementId = id,
            .detail      = "Cannot set an override without a block definition.",
        });
    }

    if (!mPlacementStore.patchBlock(id, worldPos, op)) {
        return PlacementMutationResult::failure({
            .code        = op.kind == render::PatchOp::Kind::ClearOverride
                ? PlacementError::Code::OverrideNotFound
                : PlacementError::Code::InvalidBlockOverride,
            .placementId = id,
            .detail      = op.kind == render::PatchOp::Kind::ClearOverride
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

} // namespace levishematic::app
