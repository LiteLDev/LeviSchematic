#include "ToolService.h"
#include "levischematic/render/SchematicRenderAlgorithms.h"
#include "levischematic/schematic/placement/ProjectionQuery.h"
#include "levischematic/schematic/placement/PlacementGeometry.h"

namespace levischematic::app {
bool ToolService::editRay(selection::tool::Vector const& start, selection::tool::Vector const& direction,
                          int dimension, int action) {
    auto hit = placement::traceProjection(mPlacements.orderedPlacements(), dimension,
        mState.view.layerRange, start, direction, 200);
    if (!hit) return false;
    if (action == 2) { mState.tool.sampledBlock = hit->block; ++mState.tool.revision; return true; }
    auto selected = mPlacements.selectedPlacementId();
    auto leaf=mPlacements.findPlacement(hit->owner);
    if (!selected || !leaf || *selected != (leaf->parentId?leaf->parentId:leaf->id)) return false;
    auto world = hit->position;
    if (action == 1 && mState.tool.editAdjacent) {
        double nearest = 2;
        int axis = 0, sign = 0;
        double coords[3]{double(world.x),double(world.y),double(world.z)};
        for (int i=0;i<3;++i) {
            double at = start[i]+direction[i]*hit->distance;
            double d = std::min(std::abs(at-coords[i]), std::abs(at-coords[i]-1));
            if (d < nearest) { nearest=d;axis=i;sign=direction[i] > 0 ? -1 : 1; }
        }
        auto next = placement::checkedPosition({int64_t(world.x)+(axis==0?sign:0),
            int64_t(world.y)+(axis==1?sign:0),int64_t(world.z)+(axis==2?sign:0)});
        if (!next) return false;
        world = *next;
    }
    if (action == 1 && !mState.tool.sampledBlock) return false;
    auto axis = selection::tool::lookAxis(direction,1);
    return static_cast<bool>(mPlacements.resources().edit({hit->owner,world,
        action == 0 ? nullptr : mState.tool.sampledBlock,mState.tool.editMatch,
        {axis[0],axis[1],axis[2]}},mState.view.layerRange));
}
bool ToolService::click(BlockPos pos, bool secondary, int dimension) {
    if (mSelection.isSelectionModeEnabled()) {
        return mSelection.toolClick(pos, secondary);
    }
    auto id = mPlacements.selectedPlacementId();
    auto p  = id ? mPlacements.findPlacement(*id) : nullptr;
    return p && p->dimensionId == dimension && static_cast<bool>(mPlacements.setPlacementOrigin(*id, pos));
}
bool ToolService::nudge(selection::tool::Vector const& direction, int amount, int dimension) {
    using namespace selection::tool;
    auto delta = lookAxis(direction, amount);
    if (mSelection.isSelectionModeEnabled()) return mSelection.nudgeSelected({delta[0], delta[1], delta[2]});
    auto item = mPlacements.requireSelectedPlacement();
    if (!item || item.value()->dimensionId != dimension) return false;
    auto const& origin = item.value()->origin;
    auto        next   = offset({origin.x, origin.y, origin.z}, delta);
    return next
        && static_cast<bool>(mPlacements.setPlacementOrigin(item.value()->id, {(*next)[0], (*next)[1], (*next)[2]}));
}
void ToolService::select(
    selection::tool::Vector const& start,
    selection::tool::Vector const& direction,
    int                            dimension
) {
    using namespace selection::tool;
    if (mSelection.isSelectionModeEnabled()) {
        auto const& selection=mSelection.state();
        if (selection.normal) {
            double nearest=201;uint32_t id=0;
            for (auto const& region:selection.regions) {
                if (!region.corner1 || !region.corner2) continue;
                auto hit=rayBox(start,direction,{region.corner1->x,region.corner1->y,region.corner1->z},
                    {region.corner2->x,region.corner2->y,region.corner2->z});
                if (hit && *hit<nearest) {nearest=*hit;id=region.id;}
            }
            if (id) (void)mSelection.selectRegion(id);
        }
        auto convert = [](std::optional<BlockPos> p) -> std::optional<Position> {
            if (!p) return {};
            return Position{p->x, p->y, p->z};
        };
        auto const& s = mSelection.state();
        mSelection.selectElement(selectElement(start, direction, convert(s.corner1), convert(s.corner2)));
        return;
    }
    std::optional<placement::PlacementId> selected;
    double                                nearest = 201;
    for (auto const& ref : mPlacements.orderedPlacements()) {
        auto const& p = ref.get();
        if (!p.enabled || !p.renderEnabled || p.dimensionId != dimension || !p.asset) continue;
        auto size = render::detail::rotatedStructureSize(
            {p.asset->size.x, p.asset->size.y, p.asset->size.z},
            p.rotation == placement::PlacementInstance::Rotation::CW_90
                || p.rotation == placement::PlacementInstance::Rotation::CCW_90
        );
        auto end = offset({p.origin.x, p.origin.y, p.origin.z}, {size[0] - 1, size[1] - 1, size[2] - 1});
        if (!end) continue;
        auto hit = rayBox(start, direction, {p.origin.x, p.origin.y, p.origin.z}, *end);
        if (hit && *hit < nearest) {
            nearest  = *hit;
            selected = p.id;
        }
    }
    if (selected) (void)mPlacements.selectPlacement(*selected);
    else mPlacements.clearSelectedPlacement();
}
} 
