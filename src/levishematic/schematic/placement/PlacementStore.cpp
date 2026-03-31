#include "PlacementStore.h"

#include "levishematic/render/ProjectionRenderer.h"
#include "levishematic/util/PositionUtils.h"

#include <algorithm>

namespace levishematic::placement {

PlacementId PlacementStore::createPlacement(
    std::shared_ptr<const SchematicAsset> asset,
    BlockPos                              origin,
    std::string                           name,
    std::filesystem::path                 filePath
) {
    PlacementInstance placement;
    placement.id       = mState.nextPlacementId++;
    placement.asset    = std::move(asset);
    placement.name     = name.empty() && placement.asset ? placement.asset->defaultName : std::move(name);
    placement.filePath = std::move(filePath);
    placement.origin   = origin;

    auto id = placement.id;
    mState.order.push_back(id);
    mState.placements.emplace(id, std::move(placement));
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

PlacementInstance* PlacementStore::selected() {
    return mState.selectedId ? get(*mState.selectedId) : nullptr;
}

PlacementInstance const* PlacementStore::selected() const {
    return mState.selectedId ? get(*mState.selectedId) : nullptr;
}

bool PlacementStore::remove(PlacementId id) {
    if (mState.placements.erase(id) == 0) {
        return false;
    }

    mState.order.erase(
        std::remove(mState.order.begin(), mState.order.end(), id),
        mState.order.end()
    );

    if (mState.selectedId && *mState.selectedId == id) {
        selectLast();
    }

    touchState();
    return true;
}

void PlacementStore::clear() {
    mState.placements.clear();
    mState.order.clear();
    mState.selectedId.reset();
    touchState();
}

bool PlacementStore::select(PlacementId id) {
    if (!get(id)) {
        return false;
    }
    mState.selectedId = id;
    touchState();
    return true;
}

void PlacementStore::selectLast() {
    mState.selectedId = mState.order.empty()
        ? std::optional<PlacementId>{}
        : std::optional<PlacementId>{mState.order.back()};
}

bool PlacementStore::move(PlacementId id, int dx, int dy, int dz) {
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    placement->origin.x += dx;
    placement->origin.y += dy;
    placement->origin.z += dz;
    touchPlacement(*placement);
    return true;
}

bool PlacementStore::setOrigin(PlacementId id, BlockPos origin) {
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    placement->origin = origin;
    touchPlacement(*placement);
    return true;
}

bool PlacementStore::rotate(PlacementId id, PlacementInstance::Rotation delta) {
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    placement->rotation = rotateBy(placement->rotation, delta);
    touchPlacement(*placement);
    return true;
}

bool PlacementStore::setMirror(PlacementId id, PlacementInstance::Mirror mirror) {
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    placement->mirror = mirror;
    touchPlacement(*placement);
    return true;
}

bool PlacementStore::resetTransform(PlacementId id) {
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    placement->rotation = PlacementInstance::Rotation::NONE;
    placement->mirror   = PlacementInstance::Mirror::NONE;
    touchPlacement(*placement);
    return true;
}

bool PlacementStore::toggleEnabled(PlacementId id) {
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    placement->enabled = !placement->enabled;
    touchPlacement(*placement);
    return true;
}

bool PlacementStore::toggleRender(PlacementId id) {
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    placement->renderEnabled = !placement->renderEnabled;
    touchPlacement(*placement);
    return true;
}

bool PlacementStore::patchBlock(PlacementId id, BlockPos worldPos, render::PatchOp const& op) {
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    auto posKey = util::encodePosKey(worldPos);
    switch (op.kind) {
    case render::PatchOp::Kind::Remove:
        placement->overrides[posKey] = OverrideEntry{OverrideEntry::Kind::Remove, nullptr};
        break;
    case render::PatchOp::Kind::SetBlock:
        if (!op.block) {
            return false;
        }
        placement->overrides[posKey] = OverrideEntry{OverrideEntry::Kind::SetBlock, op.block};
        break;
    case render::PatchOp::Kind::ClearOverride:
        if (placement->overrides.erase(posKey) == 0) {
            return false;
        }
        break;
    }

    touchPlacement(*placement);
    return true;
}

void PlacementStore::touchState() {
    ++mState.revision;
}

void PlacementStore::touchPlacement(PlacementInstance& placement) {
    ++placement.revision;
    touchState();
}

} // namespace levishematic::placement
