#include "ProjectionInstances.h"
#include <stdexcept>

namespace levischematic::placement {
namespace {
using R = PlacementInstance::Rotation;
using M = PlacementInstance::Mirror;
BlockPos direction(PlacementInstance const& p, BlockPos at) {
    auto a = transformLocalPos(at, p.asset->size, p.mirror, p.rotation);
    auto b = transformLocalPos({0, 0, 0}, p.asset->size, p.mirror, p.rotation);
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
PlacementInstance leaf(PlacementInstance const& base, SchematicRegion const& region, RegionSettings const& settings) {
    auto result  = base;
    result.asset = region.content;
    result.regions.clear();
    result.grid.enabled = false;
    result.overrides.clear();
    result.parentId = base.id;
    result.regionId = region.id;
    auto inner      = result;
    inner.rotation  = static_cast<R>(settings.rotation);
    inner.mirror    = static_cast<M>(settings.mirror);
    auto x          = direction(base, direction(inner, {1, 0, 0}));
    auto z          = direction(base, direction(inner, {0, 0, 1}));
    bool found      = false;
    for (int m = 0; m < 2 && !found; ++m)
        for (int r = 0; r < 4 && !found; ++r) {
            result.mirror   = static_cast<M>(m);
            result.rotation = static_cast<R>(r);
            if (direction(result, {1, 0, 0}) == x && direction(result, {0, 0, 1}) == z) found = true;
        }
    auto first = transformLocalPos({0, 0, 0}, inner.asset->size, inner.mirror, inner.rotation);
    auto at    = checkedPosition(
        {int64_t(region.offset.x) + settings.offset.x + first.x,
         int64_t(region.offset.y) + settings.offset.y + first.y,
         int64_t(region.offset.z) + settings.offset.z + first.z}
    );
    if (!at) throw std::overflow_error("Region offset overflow.");
    auto world = toWorld(base, *at);
    auto zero  = transformLocalPos({0, 0, 0}, result.asset->size, result.mirror, result.rotation);
    auto origin =
        world ? checkedPosition({int64_t(world->x) - zero.x, int64_t(world->y) - zero.y, int64_t(world->z) - zero.z})
              : std::nullopt;
    if (!origin) throw std::overflow_error("Region transform overflow.");
    result.origin = *origin;
    return result;
}
} 
void ensureProjectionInstances(PlacementState const& state) {
    std::scoped_lock lock(state.mutex);
    if (state.projectionRevision == state.revision) return;
    std::unordered_map<PlacementId, PlacementInstance> values;
    std::vector<PlacementId>                           order;
    auto                                               ids  = state.projectionIds;
    auto                                               next = state.nextProjectionId;
    for (auto id : state.order) {
        auto const& base = state.placements.at(id);
        if (!base.asset) continue;
        auto grid = base.grid;
        if (!grid.enabled) {
            grid.negative = {};
            grid.positive = {};
        }
        if (!grid.valid()) throw std::runtime_error("Invalid grid.");
        uint64_t count = std::max<size_t>(1, base.asset->regions.size());
        for (int axis = 0; axis < 3; ++axis) count *= uint64_t(grid.negative[axis]) + grid.positive[axis] + 1;
        if (count > 65536 || order.size() > 65536 - count)
            throw std::runtime_error("More than 65536 projection instances.");
        for (int x = -grid.negative[0]; x <= grid.positive[0]; ++x)
            for (int y = -grid.negative[1]; y <= grid.positive[1]; ++y)
                for (int z = -grid.negative[2]; z <= grid.positive[2]; ++z) {
                    auto append = [&](PlacementInstance p, std::string const& region) {
                        auto origin = checkedPosition(
                            {int64_t(p.origin.x) + int64_t(x) * grid.spacing[0],
                             int64_t(p.origin.y) + int64_t(y) * grid.spacing[1],
                             int64_t(p.origin.z) + int64_t(z) * grid.spacing[2]}
                        );
                        if (!origin) throw std::overflow_error("Grid coordinate overflow.");
                        bool primary = x == 0 && y == 0 && z == 0 && region.empty();
                        auto key = std::to_string(id) + ":" + region + ":" + std::to_string(x) + ":" + std::to_string(y)
                                 + ":" + std::to_string(z);
                        if (!primary) {
                            auto it = ids.find(key);
                            if (it == ids.end()) {
                                if (next == UINT32_MAX) throw std::overflow_error("Projection ID exhausted.");
                                it = ids.emplace(key, next++).first;
                            }
                            p.parentId = id;
                            p.id       = it->second;
                        }
                        p.origin  = *origin;
                        auto size = p.asset->size;
                        if (static_cast<int>(p.rotation) % 2) std::swap(size.x, size.z);
                        if (!checkedPosition(
                                {int64_t(origin->x) + std::max(1, size.x) - 1,
                                 int64_t(origin->y) + std::max(1, size.y) - 1,
                                 int64_t(origin->z) + std::max(1, size.z) - 1}
                            ))
                            throw std::overflow_error("Projection bounds overflow.");
                        order.push_back(p.id);
                        values.emplace(p.id, std::move(p));
                    };
                    if (base.asset->regions.empty()) append(base, {});
                    else
                        for (auto const& region : base.asset->regions) {
                            auto           it = base.regions.find(region.id);
                            RegionSettings settings;
                            if (it != base.regions.end()) settings = it->second;
                            if (settings.enabled) append(leaf(base, region, settings), region.id);
                        }
                }
    }
    state.projections.swap(values);
    state.projectionOrder.swap(order);
    state.projectionIds.swap(ids);
    state.nextProjectionId   = next;
    state.projectionRevision = state.revision;
}
} 
