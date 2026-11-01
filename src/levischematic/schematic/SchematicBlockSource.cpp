#include "levischematic/schematic/SchematicBlockSource.h"

#include "levischematic/util/PositionUtils.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/level/block/Block.h"
#include <stdexcept>


namespace levischematic::schematic {

SchematicBlockSource::SchematicBlockSource(
    ::Level&       level,
    ::Dimension&   dimension,
    ::ChunkSource& chunkSource,
    ::AABB const&  bounds
)
: BlockSource(level, dimension, chunkSource, false, true, false, false) {
    mBounds  = bounds;
    auto air = Block::tryGetFromRegistry(HashedString{"minecraft:air"});
    if (!air) throw std::runtime_error("Air block unavailable.");
    mAir = &*air;
}

Block const& SchematicBlockSource::getBlock(BlockPos const& pos) const {
    return projectionVisible(pos) ? BlockSource::getBlock(pos) : *mAir;
}

Block const& SchematicBlockSource::getBlock(BlockPos const& pos, uint layer) const {
    return projectionVisible(pos) ? BlockSource::getBlock(pos, layer) : *mAir;
}

Block const& SchematicBlockSource::getExtraBlock(BlockPos const& pos) const {
    return projectionVisible(pos) ? BlockSource::getExtraBlock(pos) : *mAir;
}

bool SchematicBlockSource::setProjectionVisible(BlockPos const& pos, bool visible) {
    auto key = util::encodePosKey(pos);
    return visible ? mHiddenPositions.erase(key) != 0 : mHiddenPositions.insert(key).second;
}

bool SchematicBlockSource::projectionVisible(BlockPos const& pos) const {
    return !mHiddenPositions.contains(util::encodePosKey(pos));
}

gsl::span<::gsl::not_null<::Actor*>>
SchematicBlockSource::fetchEntities(::ActorType, ::AABB const&, ::Actor const*, ::std::function<bool(::Actor*)>) {
    return {};
}

gsl::span<::gsl::not_null<::Actor*>> SchematicBlockSource::fetchEntities(::Actor const*, ::AABB const&, bool, bool) {
    return {};
}

} 
