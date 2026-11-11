#include "ProjectionQuery.h"
#include "ProjectionAlgorithms.h"
#include "PlacementGeometry.h"
#include "mc/deps/core/string/HashedString.h"
#include "VolumeIndex.h"
#include "levischematic/schematic/FakeWorld.h"
#include "levischematic/util/PositionUtils.h"
#include "levischematic/util/VoxelRay.h"
#include "mc/world/level/block/states/VanillaBlockStateTransformUtils.h"

namespace levischematic::placement {
namespace {
Block const* airBlock() {
    auto air = Block::tryGetFromRegistry(HashedString{"minecraft:air"});
    return air ? &*air : nullptr;
}
}
Block const* projectedBlock(PlacementInstance const& p, Block const* block) {
    if (!block || block->isAir()) return block;
    return VanillaBlockStateTransformUtils::transformBlock(*block,
        render::toMinecraftRotation(p.rotation), render::toMinecraftMirror(p.mirror));
}
std::optional<ExpectedPosition> expectedPosition(PlacementInstance const& p, size_t cursor) {
    if (!p.asset || cursor >= p.asset->indices.size()) return {};
    int index = p.asset->indices[cursor];
    if (index < 0) return {};
    auto size  = p.asset->size;
    auto local = volumePosition(cursor, {size.x, size.y, size.z});
    auto t     = transformLocalPos({local[0], local[1], local[2]}, size, p.mirror, p.rotation);
    auto world = selection::tool::offset({p.origin.x, p.origin.y, p.origin.z}, {t.x, t.y, t.z});
    if (!world) return {};
    BlockPos pos{(*world)[0], (*world)[1], (*world)[2]};
    if (auto it = p.overrides.find(util::encodePosKey(local[0], local[1], local[2])); it != p.overrides.end()) {
        return ExpectedPosition{pos, it->second.kind == OverrideEntry::Kind::Remove
            ? airBlock() : projectedBlock(p, it->second.block)};
    }
    auto block = static_cast<size_t>(index) < p.asset->palette.size() ? p.asset->palette[index] : nullptr;
    if (block && !block->isAir())
        block = VanillaBlockStateTransformUtils::transformBlock(
            *block,
            render::toMinecraftRotation(p.rotation),
            render::toMinecraftMirror(p.mirror)
        );
    return ExpectedPosition{pos, block, cursor};
}
std::optional<ExpectedPosition> expectedAt(PlacementInstance const& p, BlockPos const& world) {
    if (!p.asset) return {};
    auto at = toLocal(p, world);
    if (!at) return {};
    if (auto it = p.overrides.find(util::encodePosKey(*at)); it != p.overrides.end()) {
        return ExpectedPosition{world, it->second.kind == OverrideEntry::Kind::Remove
            ? airBlock() : projectedBlock(p, it->second.block)};
    }
    auto size  = p.asset->size;
    auto local = inverseProjectionPosition(
        {world.x, world.y, world.z},
        {p.origin.x, p.origin.y, p.origin.z},
        {size.x, size.y, size.z},
        static_cast<int>(p.mirror),
        static_cast<int>(p.rotation)
    );
    if (!local) return {};
    return expectedPosition(p, volumeIndex(*local, {size.x, size.y, size.z}));
}
std::optional<ProjectionHit> projectionAt(PlacementList const& list, int dimension, BlockPos const& pos) {
    std::optional<ProjectionHit> result;
    for (auto const& entry : list) {
        auto const& p = entry.get();
        if (!p.enabled || p.dimensionId != dimension) continue;
        if (auto value = expectedAt(p, pos)) {
            
            
            if ((value->block && !value->block->isAir()) || !result) result = ProjectionHit{p.id, pos, value->block, 0};
        }
    }
    return result;
}
std::optional<ProjectionHit> traceProjection(
    PlacementList const&      list,
    int                       dimension,
    editor::LayerRange const& range,
    selection::tool::Vector   start,
    selection::tool::Vector   direction,
    double                    reach
) {
    std::optional<ProjectionHit> result;
    util::traceVoxels(start, direction, reach, [&](auto const& pos, double distance) {
        BlockPos at{pos[0], pos[1], pos[2]};
        if (!range.containsPosition(at)) return true;
        auto hit = projectionAt(list, dimension, at);
        if (!hit || !hit->block || hit->block->isAir()) return true;
        hit->distance = distance;
        result        = hit;
        return false;
    });
    return result;
}
bool nearProjection(PlacementList const& list, int dimension, BlockPos const& pos, int margin) {
    for (auto const& entry : list) {
        auto const& p = entry.get();
        if (!p.enabled || p.dimensionId != dimension || !p.asset) continue;
        auto size = p.asset->size;
        if (p.rotation == PlacementInstance::Rotation::CW_90 || p.rotation == PlacementInstance::Rotation::CCW_90)
            std::swap(size.x, size.z);
        if (int64_t(pos.x) >= int64_t(p.origin.x) - margin && int64_t(pos.x) < int64_t(p.origin.x) + size.x + margin
            && int64_t(pos.y) >= int64_t(p.origin.y) - margin && int64_t(pos.y) < int64_t(p.origin.y) + size.y + margin
            && int64_t(pos.z) >= int64_t(p.origin.z) - margin && int64_t(pos.z) < int64_t(p.origin.z) + size.z + margin)
            return true;
    }
    return false;
}
} 
