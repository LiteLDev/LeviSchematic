#include "PlacementStore.h"

#include "levischematic/schematic/placement/PatchOp.h"
#include "levischematic/util/PositionUtils.h"

#include <algorithm>

namespace levischematic::placement {

PlacementId PlacementStore::createPlacement(
    std::shared_ptr<SchematicAsset const> asset,
    BlockPos                              origin,
    int                                   dimensionId,
    std::string                           name,
    std::filesystem::path                 filePath
) {
    std::scoped_lock  lock(mState.mutex);
    PlacementInstance placement;
    placement.id          = mState.nextPlacementId;
    placement.asset       = std::move(asset);
    placement.name        = name.empty() && placement.asset ? placement.asset->defaultName : std::move(name);
    placement.filePath    = std::move(filePath);
    placement.dimensionId = dimensionId;
    placement.origin      = origin;

    auto id = placement.id;
    mState.placements.emplace(id, std::move(placement));
    try {
        mState.order.push_back(id);
    } catch (...) {
        mState.placements.erase(id);
        throw;
    }
    ++mState.nextPlacementId;
    mState.selectedId = id;
    touchState();
    return id;
}

PlacementInstance* PlacementStore::get(PlacementId id) {
    auto it = mState.placements.find(id);
    return it == mState.placements.end() ? nullptr : &it->second;
}

PlacementInstance const* PlacementStore::get(PlacementId id) const {
    auto it = mState.placements.find(id);
    return it == mState.placements.end() ? nullptr : &it->second;
}

PlacementInstance* PlacementStore::selected() { return mState.selectedId ? get(*mState.selectedId) : nullptr; }

PlacementInstance const* PlacementStore::selected() const {
    return mState.selectedId ? get(*mState.selectedId) : nullptr;
}

bool PlacementStore::remove(PlacementId id) {
    std::scoped_lock lock(mState.mutex);
    if (mState.placements.erase(id) == 0) {
        return false;
    }

    mState.order.erase(std::remove(mState.order.begin(), mState.order.end(), id), mState.order.end());

    if (mState.selectedId && *mState.selectedId == id) {
        selectLast();
    }

    touchState();
    return true;
}

void PlacementStore::clear() {
    std::scoped_lock lock(mState.mutex);
    mState.placements.clear();
    mState.order.clear();
    mState.selectedId.reset();
    touchState();
}

bool PlacementStore::select(PlacementId id) {
    std::scoped_lock lock(mState.mutex);
    if (!get(id)) {
        return false;
    }
    mState.selectedId = id;
    touchState();
    return true;
}

void PlacementStore::selectLast() {
    mState.selectedId =
        mState.order.empty() ? std::optional<PlacementId>{} : std::optional<PlacementId>{mState.order.back()};
}

bool PlacementStore::move(PlacementId id, int dx, int dy, int dz) {
    std::scoped_lock lock(mState.mutex);
    auto*            placement = get(id);
    if (!placement) {
        return false;
    }

    placement->origin.x += dx;
    placement->origin.y += dy;
    placement->origin.z += dz;
    touchPlacement(*placement, !placement->overrides.empty());
    return true;
}

bool PlacementStore::setOrigin(PlacementId id, BlockPos origin) {
    std::scoped_lock lock(mState.mutex);
    auto*            placement = get(id);
    if (!placement) {
        return false;
    }

    placement->origin = origin;
    touchPlacement(*placement, !placement->overrides.empty());
    return true;
}

bool PlacementStore::rotate(PlacementId id, PlacementInstance::Rotation delta) {
    std::scoped_lock lock(mState.mutex);
    auto*            placement = get(id);
    if (!placement) {
        return false;
    }

    placement->rotation = rotateBy(placement->rotation, delta);
    touchPlacement(*placement);
    return true;
}

bool PlacementStore::setMirror(PlacementId id, PlacementInstance::Mirror mirror) {
    std::scoped_lock lock(mState.mutex);
    auto*            placement = get(id);
    if (!placement) {
        return false;
    }

    placement->mirror = mirror;
    touchPlacement(*placement);
    return true;
}

bool PlacementStore::resetTransform(PlacementId id) {
    std::scoped_lock lock(mState.mutex);
    auto*            placement = get(id);
    if (!placement) {
        return false;
    }

    placement->rotation = PlacementInstance::Rotation::NONE;
    placement->mirror   = PlacementInstance::Mirror::NONE;
    touchPlacement(*placement);
    return true;
}

bool PlacementStore::toggleEnabled(PlacementId id) {
    std::scoped_lock lock(mState.mutex);
    auto*            placement = get(id);
    if (!placement) {
        return false;
    }

    placement->enabled = !placement->enabled;
    touchPlacement(*placement);
    return true;
}

bool PlacementStore::toggleRender(PlacementId id) {
    std::scoped_lock lock(mState.mutex);
    auto*            placement = get(id);
    if (!placement) {
        return false;
    }

    placement->renderEnabled = !placement->renderEnabled;
    touchPlacement(*placement);
    return true;
}

bool PlacementStore::patchBlock(PlacementId id, BlockPos worldPos, placement::PatchOp const& op) {
    std::scoped_lock lock(mState.mutex);
    auto*            placement = get(id);
    if (!placement) {
        return false;
    }

    auto posKey = util::encodePosKey(worldPos);
    switch (op.kind) {
    case placement::PatchOp::Kind::Remove:
        placement->overrides[posKey] = OverrideEntry{OverrideEntry::Kind::Remove, nullptr};
        break;
    case placement::PatchOp::Kind::SetBlock:
        if (!op.block) {
            return false;
        }
        placement->overrides[posKey] = OverrideEntry{OverrideEntry::Kind::SetBlock, op.block};
        break;
    case placement::PatchOp::Kind::ClearOverride:
        if (placement->overrides.erase(posKey) == 0) {
            return false;
        }
        break;
    }

    touchPlacement(*placement);
    return true;
}

void PlacementStore::touchState() { ++mState.revision; }

void PlacementStore::touchPlacement(PlacementInstance& placement, bool contentChanged) {
    ++placement.revision;
    if (contentChanged) ++placement.contentRevision;
    touchState();
}

} 
