#include "SchematicBlockEntities.h"

#include "FakeWorld.h"
#include "levischematic/LeviSchematic.h"
#include "levischematic/schematic/placement/PlacementModel.h"
#include "levischematic/util/PositionUtils.h"

#include "mc/dataloadhelper/StructureDataLoadHelper.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/level/ChunkPos.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/block/actor/ChestBlockActor.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/chunk/SubChunk.h"
#include "mc/world/level/levelgen/structure/StructureBlockPalette.h"
#include "mc/world/level/levelgen/structure/StructureSettings.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include "mc/world/level/levelgen/structure/StructureTemplateData.h"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace levischematic::render {

void createSchematicBlockEntity(
    placement::PlacementInstance const& placement,
    FakeWorld&                          world,
    BlockPos                            pos,
    Block const*                        block,
    std::optional<BlockPos>             source
) {
    if (!block || block->getBlockType().getBlockEntityType() == BlockActorType::Undefined) return;
    auto&                   region = *world.blockSource;
    auto const&             data   = placement.asset->structure->mStructureTemplateData.get();
    auto                    size   = placement.asset->size;
    StructureDataLoadHelper helper;
    helper.mStructurePlacementLocation = BlockPos::ZERO();
    helper.mStructureWorldOrigin       = data.mStructureWorldOrigin;
    helper.mPivot                      = Vec3{(size.x - 1) * 0.5f, (size.y - 1) * 0.5f, (size.z - 1) * 0.5f};
    helper.mOwner                      = ActorUniqueID::INVALID_ID();
    helper.mRotation                   = toMinecraftRotation(placement.rotation);
    helper.mMirror                     = toMinecraftMirror(placement.mirror);
    helper.mLevel                      = &region.getLevel();
    helper.mOldIDToNewID               = {};
    CompoundTag const* nbt             = nullptr;
    if (source) {
        auto const* palette = data.getPalette(StructureTemplateData::DEFAULT_PALETTE_NAME());
        auto        index   = static_cast<uint64_t>(source->z)
                            + static_cast<uint64_t>(size.z) * (source->y + static_cast<uint64_t>(size.y) * source->x);
        auto const* positionData = palette ? palette->getBlockPositionData(index) : nullptr;
        if (positionData && !placement.asset->editedCells.contains(index)) nbt = positionData->mBlockEntityData.get();
    }
    CompoundTag empty;
    auto actor = BlockActor::loadStatic(block->getBlockType(), pos, region.getLevel(), nbt ? *nbt : empty, helper);
    if (!actor) return;
    auto* chest = actor->getType() == BlockActorType::Chest ? static_cast<ChestBlockActor*>(actor.get()) : nullptr;
    auto  chunk = world.chunkSource->getExistingChunk(ChunkPos{pos.x >> 4, pos.z >> 4});
    if (!chunk) throw std::runtime_error("Missing block entity chunk.");
    chunk->_placeBlockEntity(std::move(actor));
    if (!chest) return;
    auto resolvePair = [&](ChestBlockActor& lead) {
        if (!lead.mDeferredPairLoad || lead.mLargeChestPaired) return;
        auto const& leadPos = lead.getPosition();
        BlockPos    pairedPos{lead.mDeferredPairX, leadPos.y, lead.mDeferredPairZ};
        if (std::abs(leadPos.x - pairedPos.x) + std::abs(leadPos.z - pairedPos.z) != 1) return;
        auto* other = region.getBlockEntity(pairedPos);
        if (!other || !lead.canPairWith(other, region)) return;
        auto* paired = static_cast<ChestBlockActor*>(other);
        paired->_pairWith(&lead, region, false);
        lead._pairWith(paired, region, true);
        lead.mDeferredPairLoad    = false;
        paired->mDeferredPairLoad = false;
        world.markDirty(leadPos);
        world.markDirty(pairedPos);
    };
    resolvePair(*chest);
    
    
    
    for (auto const offset : {
             BlockPos{1,  0, 0 },
             BlockPos{-1, 0, 0 },
             BlockPos{0,  0, 1 },
             BlockPos{0,  0, -1}
    }) {
        auto* neighbor = region.getBlockEntity({pos.x + offset.x, pos.y, pos.z + offset.z});
        if (!neighbor || neighbor->getType() != BlockActorType::Chest) continue;
        auto& lead = *static_cast<ChestBlockActor*>(neighbor);
        if (lead.mDeferredPairLoad && lead.mDeferredPairX == pos.x && lead.mDeferredPairZ == pos.z) resolvePair(lead);
    }
}
} 
