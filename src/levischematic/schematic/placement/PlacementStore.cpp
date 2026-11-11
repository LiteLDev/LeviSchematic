#include "PlacementStore.h"
#include "PlacementGeometry.h"
#include "ProjectionInstances.h"

#include "levischematic/schematic/placement/PatchOp.h"
#include "levischematic/util/PositionUtils.h"

#include <algorithm>
#include <stdexcept>

namespace levischematic::placement {

void PlacementStore::replaceDimension(
    int                            dimension,
    std::vector<PlacementInstance> restored,
    std::optional<size_t>          selected
) {
    std::scoped_lock lock(mState.mutex);
    auto             placements = mState.placements;
    auto             order      = mState.order;
    std::erase_if(order, [&](auto id) { return placements.at(id).dimensionId == dimension; });
    std::erase_if(placements, [&](auto const& item) { return item.second.dimensionId == dimension; });
    auto                       next = mState.nextPlacementId;
    std::optional<PlacementId> selection;
    for (size_t i = 0; i < restored.size(); ++i) {
        if (next == 0 || next >= 0x80000000u) throw std::overflow_error("Placement ID exhausted.");
        auto& value       = restored[i];
        value.id          = next++;
        value.dimensionId = dimension;
        value.revision = value.contentRevision = 1;
        if (selected == i) selection = value.id;
        order.push_back(value.id);
        placements.emplace(value.id, std::move(value));
    }
    PlacementState staged;
    staged.placements = placements;
    staged.order = order;
    staged.projectionIds = mState.projectionIds;
    staged.nextProjectionId = mState.nextProjectionId;
    ensureProjectionInstances(staged);
    mState.placements.swap(placements);
    mState.order.swap(order);
    mState.nextPlacementId = next;
    mState.selectedId      = selection;
    touchState();
}

PlacementId PlacementStore::createPlacement(
    std::shared_ptr<const SchematicAsset> asset,
    BlockPos                              origin,
    int                                   dimensionId,
    std::string                           name,
    std::filesystem::path                 filePath
) {
    std::scoped_lock lock(mState.mutex);
    PlacementInstance placement;
    if (mState.nextPlacementId==0 || mState.nextPlacementId>=0x80000000u) throw std::overflow_error("Placement ID exhausted.");
    placement.id       = mState.nextPlacementId;
    placement.asset    = std::move(asset);
    placement.name     = name.empty() && placement.asset ? placement.asset->defaultName : std::move(name);
    placement.filePath = std::move(filePath);
    placement.dimensionId = dimensionId;
    placement.origin   = origin;

    auto id = placement.id;
    mState.placements.emplace(id, std::move(placement));
    try { mState.order.push_back(id); }
    catch (...) { mState.placements.erase(id); throw; }
    auto previousRevision=mState.revision;
    touchState();
    try { ensureProjectionInstances(mState); }
    catch (...) {
        mState.order.pop_back();mState.placements.erase(id);mState.revision=previousRevision;
        throw;
    }
    ++mState.nextPlacementId;
    mState.selectedId = id;
    return id;
}

PlacementInstance* PlacementStore::get(PlacementId id) {
    auto it = mState.placements.find(id);
    if (it != mState.placements.end()) return &it->second;
    ensureProjectionInstances(mState);
    auto view=mState.projections.find(id);
    return view==mState.projections.end()?nullptr:&view->second;
}

PlacementInstance const* PlacementStore::get(PlacementId id) const {
    auto it = mState.placements.find(id);
    if (it != mState.placements.end()) return &it->second;
    ensureProjectionInstances(mState);
    auto view=mState.projections.find(id);
    return view==mState.projections.end()?nullptr:&view->second;
}

PlacementInstance* PlacementStore::selected() {
    return mState.selectedId ? get(*mState.selectedId) : nullptr;
}

PlacementInstance const* PlacementStore::selected() const {
    return mState.selectedId ? get(*mState.selectedId) : nullptr;
}

bool PlacementStore::remove(PlacementId id) {
    std::scoped_lock lock(mState.mutex);
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
    std::scoped_lock lock(mState.mutex);
    mState.placements.clear();
    mState.order.clear();
    mState.selectedId.reset();
    touchState();
}

bool PlacementStore::select(PlacementId id) {
    std::scoped_lock lock(mState.mutex);
    auto* p=get(id);
    if (!p) {
        return false;
    }
    mState.selectedId = p->parentId ? p->parentId : id;
    touchState();
    return true;
}

void PlacementStore::selectLast() {
    mState.selectedId = mState.order.empty()
        ? std::optional<PlacementId>{}
        : std::optional<PlacementId>{mState.order.back()};
}

void PlacementStore::deselect() {
    std::scoped_lock lock(mState.mutex);
    if (!mState.selectedId) return;
    mState.selectedId.reset();
    touchState();
}

bool PlacementStore::move(PlacementId id, int dx, int dy, int dz) {
    std::scoped_lock lock(mState.mutex);
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    auto next = checkedPosition({int64_t(placement->origin.x) + dx,
                                 int64_t(placement->origin.y) + dy,
                                 int64_t(placement->origin.z) + dz});
    return next && setOrigin(id, *next);
}

bool PlacementStore::setOrigin(PlacementId id, BlockPos origin) {
    std::scoped_lock lock(mState.mutex);
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    if (placement->parentId || placement->locked || (placement->axisLocked[0] && origin.x != placement->origin.x)
        || (placement->axisLocked[1] && origin.y != placement->origin.y)
        || (placement->axisLocked[2] && origin.z != placement->origin.z)) return false;
    return commitTransform(*placement,origin,placement->rotation,placement->mirror,false);
}

bool PlacementStore::rotate(PlacementId id, PlacementInstance::Rotation delta) {
    std::scoped_lock lock(mState.mutex);
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    if (placement->parentId || placement->locked) return false;
    return commitTransform(*placement,placement->origin,rotateBy(placement->rotation,delta),placement->mirror,true);
}

bool PlacementStore::setMirror(PlacementId id, PlacementInstance::Mirror mirror) {
    std::scoped_lock lock(mState.mutex);
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    if (placement->parentId || placement->locked) return false;
    return commitTransform(*placement,placement->origin,placement->rotation,mirror,true);
}

bool PlacementStore::resetTransform(PlacementId id) {
    std::scoped_lock lock(mState.mutex);
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    if (placement->parentId || placement->locked) return false;
    return commitTransform(*placement,placement->origin,PlacementInstance::Rotation::NONE,PlacementInstance::Mirror::NONE,true);
}

bool PlacementStore::toggleEnabled(PlacementId id) {
    std::scoped_lock lock(mState.mutex);
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    placement->enabled = !placement->enabled;
    touchPlacement(*placement);
    return true;
}

bool PlacementStore::toggleRender(PlacementId id) {
    std::scoped_lock lock(mState.mutex);
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    placement->renderEnabled = !placement->renderEnabled;
    touchPlacement(*placement, false);
    return true;
}

bool PlacementStore::patchBlock(PlacementId id, BlockPos worldPos, placement::PatchOp const& op) {
    std::scoped_lock lock(mState.mutex);
    auto* placement = get(id);
    if (!placement) {
        return false;
    }

    auto local = toLocal(*placement, worldPos);
    if (!local) return false;
    if (util::decodePosKey(util::encodePosKey(*local))!=*local) return false;
    auto posKey = util::encodePosKey(*local);
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

bool PlacementStore::rename(PlacementId id, std::string name) {
    std::scoped_lock lock(mState.mutex);
    auto* p = get(id);
    if (!p || name.empty()) return false;
    p->name = std::move(name);
    touchPlacement(*p, false);
    return true;
}
bool PlacementStore::setLocks(PlacementId id, bool locked, std::array<bool, 3> axes) {
    std::scoped_lock lock(mState.mutex);
    auto* p = get(id);
    if (!p) return false;
    p->locked = locked;
    p->axisLocked = axes;
    touchPlacement(*p, false);
    return true;
}
bool PlacementStore::setEntitiesEnabled(PlacementId id, bool enabled) {
    std::scoped_lock lock(mState.mutex);
    auto* p = get(id);
    if (!p) return false;
    p->entitiesEnabled = enabled;
    touchPlacement(*p, false);
    return true;
}
bool PlacementStore::setGrid(PlacementId id, GridSettings grid) {
    std::scoped_lock lock(mState.mutex);
    auto* p=get(id);
    if (!p || p->parentId || p->locked || !grid.valid()) return false;
    auto previous=p->grid;
    p->grid=grid;
    ++mState.revision;
    try { ensureProjectionInstances(mState); }
    catch (...) { p->grid=previous; --mState.revision; return false; }
    touchPlacement(*p);
    return true;
}
bool PlacementStore::setRegion(PlacementId id, std::string const& region, RegionSettings settings) {
    std::scoped_lock lock(mState.mutex);
    auto* p=get(id);
    if (!p || p->parentId || p->locked || !p->asset || settings.rotation<0 || settings.rotation>3
        || settings.mirror<0 || settings.mirror>2) return false;
    if (std::none_of(p->asset->regions.begin(),p->asset->regions.end(),[&](auto const& r){return r.id==region;})) return false;
    auto previous=p->regions;
    p->regions[region]=settings;
    ++mState.revision;
    try { ensureProjectionInstances(mState); }
    catch (...) { p->regions=std::move(previous); --mState.revision;return false; }
    touchPlacement(*p);
    return true;
}
void PlacementStore::replaceAsset(std::shared_ptr<const SchematicAsset> const& previous,
                                 std::shared_ptr<const SchematicAsset> next) {
    std::scoped_lock lock(mState.mutex);
    PlacementState staged;
    staged.placements=mState.placements;staged.order=mState.order;
    staged.projectionIds=mState.projectionIds;staged.nextProjectionId=mState.nextProjectionId;
    for(auto& [id,p]:staged.placements)if(p.asset==previous)p.asset=next;
    ensureProjectionInstances(staged);
    for (auto& [id, p] : mState.placements) {
        if (p.asset != previous) continue;
        p.asset = next;
        touchPlacement(p);
    }
}
bool PlacementStore::bindAsset(PlacementId id,std::shared_ptr<const SchematicAsset> asset,std::filesystem::path path) {
    std::scoped_lock lock(mState.mutex);
    auto it=mState.placements.find(id);
    if (it==mState.placements.end() || !asset) return false;
    PlacementState staged;
    staged.placements=mState.placements;staged.order=mState.order;
    staged.projectionIds=mState.projectionIds;staged.nextProjectionId=mState.nextProjectionId;
    staged.placements.at(id).asset=asset;
    try {ensureProjectionInstances(staged);}catch(...){return false;}
    it->second.asset=std::move(asset);it->second.filePath=std::move(path);
    touchPlacement(it->second);return true;
}

bool PlacementStore::commitTransform(PlacementInstance& p,BlockPos origin,PlacementInstance::Rotation rotation,
                                      PlacementInstance::Mirror mirror,bool contentChanged) {
    auto oldOrigin=p.origin;auto oldRotation=p.rotation;auto oldMirror=p.mirror;
    auto revision=p.revision,content=p.contentRevision,stateRevision=mState.revision;
    p.origin=origin;p.rotation=rotation;p.mirror=mirror;
    touchPlacement(p,contentChanged);
    try {ensureProjectionInstances(mState);return true;}
    catch (...) {
        p.origin=oldOrigin;p.rotation=oldRotation;p.mirror=oldMirror;
        p.revision=revision;p.contentRevision=content;mState.revision=stateRevision;
        mState.projectionRevision=UINT64_MAX;return false;
    }
}

void PlacementStore::touchState() {
    ++mState.revision;
}

void PlacementStore::touchPlacement(PlacementInstance& placement, bool contentChanged) {
    ++placement.revision;
    if (contentChanged) ++placement.contentRevision;
    touchState();
}

} 
