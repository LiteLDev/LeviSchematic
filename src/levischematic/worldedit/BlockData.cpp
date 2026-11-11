#include "BlockData.h"
#include "levischematic/schematic/FakeWorld.h"
#include "levischematic/schematic/SchematicBlockEntities.h"
#include "levischematic/survival/BlockItems.h"
#include "levischematic/survival/InventoryAccess.h"
#include "levischematic/util/PositionUtils.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/level/ChunkBlockPos.h"
#include "mc/world/level/ChunkPos.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/dimension/Dimension.h"
#include <stdexcept>

namespace levischematic::worldedit {
ItemStack copyItem(ItemStackBase const& item) {
    ItemStack result;
    if (!item || !item.mItem) return result;
    result.reinit(*item.mItem, item.mCount, item.getAuxValue());
    if (item.mUserData) result.setUserData(item.mUserData->clone());
    return result;
}
ItemStack worldBlockItem(BlockSource& source, BlockPos const& pos) {
    auto const& block = source.getBlock(pos);
    if (block.isAir()) return {};
    return copyItem(block.asItemInstance(source, pos, true));
}
ItemStack
projectionBlockItem(placement::PlacementInstance const& p, placement::ExpectedPosition const& cell, Dimension& dim) {
    if (!cell.block || cell.block->isAir()) return {};
    if (cell.block->getBlockType().getBlockEntityType() == BlockActorType::Undefined) {
        auto resolved = survival::resolveBlockItems(p, cell);
        return std::move(resolved.pick);
    }
    
    render::FakeWorld scratch(p, dim);
    BlockPos          local{cell.world.x - p.origin.x, cell.world.y - p.origin.y, cell.world.z - p.origin.z};
    if (local.y < dim.mHeightRange->mMin || local.y >= dim.mHeightRange->mMax)
        throw std::runtime_error("Block data sample exceeds dimension height.");
    auto chunk =
        scratch.chunkSource->getOrLoadChunk(ChunkPos{local.x >> 4, local.z >> 4}, ChunkSource::LoadMode{}, true);
    if (!chunk) throw std::runtime_error("Cannot allocate data sample chunk.");
    chunk->setBlockSimple(ChunkBlockPos{local, static_cast<short>(dim.mHeightRange->mMin)}, *cell.block);
    std::optional<BlockPos> source;
    if (cell.sourceIndex != SIZE_MAX) {
        auto size = p.asset->size;
        auto i    = cell.sourceIndex;
        source    = BlockPos{
            static_cast<int>(i / (uint64_t(size.y) * size.z)),
            static_cast<int>((i / size.z) % size.y),
            static_cast<int>(i % size.z)
        };
    }
    render::createSchematicBlockEntity(p, scratch, local, cell.block, source);
    if (!scratch.blockSource->getBlockEntity(local))
        throw std::runtime_error("Projection block entity could not be sampled.");
    return worldBlockItem(*scratch.blockSource, local);
}
bool blockMatches(BlockSource& source, BlockPos const& pos, Block const* expected, ItemStack const& data) {
    if (survival::compareBlocks(expected, &source.getBlock(pos)) != verifier::VerificationStatus::Matched) return false;
    if (!expected || expected->isAir() || expected->getBlockType().getBlockEntityType() == BlockActorType::Undefined)
        return true;
    auto item = worldBlockItem(source, pos);
    return survival::buildItemMatches(item, data, survival::ItemMatch::Complete);
}
} 
