#include "PlacementService.h"

#include "levischematic/schematic/placement/PatchOp.h"
#include "levischematic/schematic/placement/SchematicContent.h"
#include "levischematic/schematic/placement/ProjectionInstances.h"

namespace levischematic::app {
void PlacementService::prepareScan(placement::ProjectionScan& scan, placement::PlacementId id, bool includeGrid) const {
    if (includeGrid) {
        scan.reset(mState.placements, id);
        return;
    }
    auto                      lock = lockPlacements();
    placement::PlacementState isolated;
    auto                      p = findPlacement(id);
    if (!p) {
        scan = {};
        return;
    }
    auto copy         = *p;
    copy.grid.enabled = false;
    isolated.placements.emplace(id, std::move(copy));
    isolated.order.push_back(id);
    scan.reset(isolated, id);
}
void PlacementService::clearSelectedPlacement() { mPlacementStore.deselect(); }

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
    , mPlacementStore(placementStore), mResources(placementStore,state.resources) {}

PlacementLoadInfo PlacementService::adoptAsset(std::shared_ptr<const placement::SchematicAsset> asset,
                                              std::filesystem::path const& path, BlockPos origin, int dimensionId) {
    PlacementLoadInfo result{.resolvedPath = path, .name = asset->defaultName};
    asset = mResources.adopt(std::move(asset), path);
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

std::vector<std::reference_wrapper<placement::PlacementInstance const>> PlacementService::managedPlacements() const {
    std::vector<std::reference_wrapper<placement::PlacementInstance const>> placements;
    placements.reserve(mState.placements.order.size());

    for (auto placementId : mState.placements.order) {
        if (auto const* placement = mPlacementStore.get(placementId)) {
            placements.emplace_back(*placement);
        }
    }

    return placements;
}

std::vector<std::reference_wrapper<placement::PlacementInstance const>> PlacementService::orderedPlacements() const {
    placement::ensureProjectionInstances(mState.placements);
    std::vector<std::reference_wrapper<placement::PlacementInstance const>> result;
    for (auto id : mState.placements.projectionOrder) result.emplace_back(mState.placements.projections.at(id));
    return result;
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

    if (!mPlacementStore.move(id, dx, dy, dz))
        return PlacementMutationResult::failure(makeNotFoundError(id, "Placement locked, axis locked or coordinate overflow."));
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::setPlacementOrigin(placement::PlacementId id, BlockPos origin) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    if (!mPlacementStore.setOrigin(id, origin))
        return PlacementMutationResult::failure(makeNotFoundError(id, "Placement or coordinate axis is locked."));
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

    if (!mPlacementStore.rotate(id, delta))
        return PlacementMutationResult::failure(makeNotFoundError(id, "Placement is locked."));
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

    if (!mPlacementStore.setMirror(id, mirror))
        return PlacementMutationResult::failure(makeNotFoundError(id, "Placement is locked."));
    return successMutation(mPlacementStore.get(id));
}

PlacementMutationResult PlacementService::resetPlacementTransform(placement::PlacementId id) {
    auto placement = requirePlacement(id);
    if (!placement) {
        return PlacementMutationResult::failure(placement.error());
    }

    if (!mPlacementStore.resetTransform(id))
        return PlacementMutationResult::failure(makeNotFoundError(id, "Placement is locked."));
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

    auto local = op;
    if (op.kind == placement::PatchOp::Kind::SetBlock) local.block = placement::localBlock(*placement.value(), op.block);
    if (!(mPlacementStore.patchBlock(id, worldPos, local))) {
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

PlacementMutationResult PlacementService::renamePlacement(placement::PlacementId id, std::string name) {
    if (!mPlacementStore.rename(id, std::move(name))) return PlacementMutationResult::failure(makeNotFoundError(id, "Invalid placement/name."));
    return successMutation(mPlacementStore.get(id));
}
PlacementMutationResult PlacementService::setPlacementGrid(placement::PlacementId id, placement::GridSettings grid) {
    if (!mPlacementStore.setGrid(id, grid)) return PlacementMutationResult::failure(makeNotFoundError(id, "Locked placement, invalid/overflowing grid or more than 65536 instances."));
    return successMutation(mPlacementStore.get(id));
}
PlacementMutationResult PlacementService::setPlacementRegion(placement::PlacementId id, std::string const& region, placement::RegionSettings value) {
    if (!mPlacementStore.setRegion(id,region,value)) return PlacementMutationResult::failure(makeNotFoundError(id, "Locked placement or invalid region transform."));
    return successMutation(mPlacementStore.get(id));
}
PlacementMutationResult PlacementService::setPlacementLocks(placement::PlacementId id, bool locked, std::array<bool, 3> axes) {
    if (!mPlacementStore.setLocks(id, locked, axes)) return PlacementMutationResult::failure(makeNotFoundError(id));
    return successMutation(mPlacementStore.get(id));
}
PlacementMutationResult PlacementService::setPlacementEntities(placement::PlacementId id, bool enabled) {
    if (!mPlacementStore.setEntitiesEnabled(id, enabled)) return PlacementMutationResult::failure(makeNotFoundError(id));
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
