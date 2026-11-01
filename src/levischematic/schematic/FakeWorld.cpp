#include "FakeWorld.h"
#include "SchematicBlockEntities.h"
#include "levischematic/render/SchematicRenderAlgorithms.h"
#include "mc/world/level/ChunkBlockPos.h"
#include "mc/world/level/ChunkPos.h"
#include "mc/world/level/ILevel.h"
#include "mc/world/level/block/states/VanillaBlockStateTransformUtils.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/dimension/Dimension.h"
#include <algorithm>
#include <stdexcept>

namespace levischematic::render {
BlockPos transformedSizeFor(placement::PlacementInstance const& placement) {
    auto const size               = placement.asset->size;
    bool const swapHorizontalAxes = placement.rotation == placement::PlacementInstance::Rotation::CW_90
                                 || placement.rotation == placement::PlacementInstance::Rotation::CCW_90;

    auto const rotated = detail::rotatedStructureSize({size.x, size.y, size.z}, swapHorizontalAxes);
    return {rotated[0], rotated[1], rotated[2]};
}

AABB placementRenderBounds(placement::PlacementInstance const& placement) {
    auto size = transformedSizeFor(placement);
    return AABB{
        Vec3::ZERO(),
        Vec3{static_cast<float>(size.x), static_cast<float>(size.y), static_cast<float>(size.z)}
    };
}


FakeWorld::FakeWorld(placement::PlacementInstance const& placement, Dimension& dimension)
: transformedSize(transformedSizeFor(placement)),
  mMinHeight(dimension.mHeightRange->mMin) {
    chunkSource =
        std::make_unique<schematic::SchematicChunkSource>(dimension, 0, std::vector<std::unique_ptr<LevelChunk>>{});
    blockSource = std::make_unique<schematic::SchematicBlockSource>(
        *dimension.mLevel.asLevel(),
        dimension,
        *chunkSource,
        placementRenderBounds(placement)
    );
    mOverride = placement.overrides.begin();
}
void FakeWorld::append(BlockPos pos, Block const* block, std::optional<BlockPos> source) {
    if (!block || block->isAir()) return;
    auto& bounds        = blockSource->mBounds;
    bounds.min.x        = std::min(bounds.min.x, static_cast<float>(pos.x));
    bounds.min.y        = std::min(bounds.min.y, static_cast<float>(pos.y));
    bounds.min.z        = std::min(bounds.min.z, static_cast<float>(pos.z));
    bounds.max.x        = std::max(bounds.max.x, static_cast<float>(pos.x) + 1.0f);
    bounds.max.y        = std::max(bounds.max.y, static_cast<float>(pos.y) + 1.0f);
    bounds.max.z        = std::max(bounds.max.z, static_cast<float>(pos.z) + 1.0f);
    auto key            = util::subChunkKeyFromWorldPos(pos.x, pos.y, pos.z);
    auto [it, inserted] = subChunks.try_emplace(key);
    if (inserted) origins.push_back(util::subChunkOrigin(pos.x, pos.y, pos.z));
    it->second.blocks.push_back({pos, block, source});
}
void FakeWorld::write(BlockPos pos, Block const& block) {
    auto chunk = chunkSource->getOrLoadChunk(ChunkPos{pos.x >> 4, pos.z >> 4}, ChunkSource::LoadMode{}, true);
    if (!chunk) throw std::runtime_error("Cannot allocate schematic chunk.");
    chunk->setBlockSimple(ChunkBlockPos{pos, static_cast<short>(mMinHeight)}, block);
}
bool FakeWorld::visible(
    placement::PlacementInstance const& placement,
    verifier::VerifierState const&      verification,
    editor::ViewState const&            view,
    BlockPos                            pos
) const {
    BlockPos world{placement.origin.x + pos.x, placement.origin.y + pos.y, placement.origin.z + pos.z};
    return view.layerRange.contains(world.y)
        && verification.showsProjection(placement.dimensionId, world, placement.id);
}
uint64_t FakeWorld::visibilityStamp(
    placement::PlacementInstance const& placement,
    verifier::VerifierState const&      verification,
    BlockPos                            localOrigin
) const {
    uint64_t stamp = 0;
    for (int x : {0, 15})
        for (int y : {0, 15})
            for (int z : {0, 15}) {
                auto key = util::subChunkKeyFromWorldPos(
                    placement.origin.x + localOrigin.x + x,
                    placement.origin.y + localOrigin.y + y,
                    placement.origin.z + localOrigin.z + z
                );
                auto it = verification.chunks.find({placement.dimensionId, key});
                if (!verification.clearing && it != verification.chunks.end())
                    stamp = std::max(stamp, it->second.revision);
            }
    return stamp;
}
void FakeWorld::markDirty(BlockPos pos) {
    auto origin = util::subChunkOrigin(pos.x, pos.y, pos.z);
    for (BlockPos offset : {
             BlockPos{0,   0,   0  },
             BlockPos{16,  0,   0  },
             BlockPos{-16, 0,   0  },
             BlockPos{0,   16,  0  },
             BlockPos{0,   -16, 0  },
             BlockPos{0,   0,   16 },
             BlockPos{0,   0,   -16}
    })
        dirty.insert(util::encodeSubChunkKey({origin.x + offset.x, origin.y + offset.y, origin.z + offset.z}));
}
void FakeWorld::advance(
    placement::PlacementInstance const& placement,
    verifier::VerifierState const&      verification,
    editor::ViewState const&            view,
    util::WorkBudget const&             budget
) {
    
    while (!indexed && budget.available()) {
        if (mIndex < placement.asset->localBlocks.size()) {
            auto const& entry = placement.asset->localBlocks[mIndex++];
            auto        pos   = placement::transformLocalPos(
                entry.localPos,
                placement.asset->size,
                placement.mirror,
                placement.rotation
            );
            BlockPos world{placement.origin.x + pos.x, placement.origin.y + pos.y, placement.origin.z + pos.z};
            if (placement.overrides.contains(util::encodePosKey(world))) continue;
            auto* block = VanillaBlockStateTransformUtils::transformBlock(
                *entry.renderBlock,
                toMinecraftRotation(placement.rotation),
                toMinecraftMirror(placement.mirror)
            );
            append(pos, block, entry.localPos);
        } else if (mOverride != placement.overrides.end()) {
            auto const& [key, replacement] = *mOverride++;
            if (replacement.kind != placement::OverrideEntry::Kind::SetBlock) continue;
            auto pos = util::decodePosKey(key);
            append(
                {pos.x - placement.origin.x, pos.y - placement.origin.y, pos.z - placement.origin.z},
                replacement.block,
                {}
            );
        } else indexed = true;
    }
    while (indexed && mBuild < origins.size() && budget.available()) {
        auto  origin = origins[mBuild];
        auto& chunk  = subChunks.at(util::encodeSubChunkKey(origin));
        if (chunk.cursor < chunk.blocks.size()) {
            auto& entry = chunk.blocks[chunk.cursor++];
            write(entry.pos, *entry.block);
            createSchematicBlockEntity(placement, *this, entry.pos, entry.block, entry.source);
            if (blockSource->setProjectionVisible(entry.pos, visible(placement, verification, view, entry.pos)))
                write(entry.pos, blockSource->getBlock(entry.pos));
        } else {
            chunk.ready  = true;
            chunk.built  = true;
            chunk.cursor = 0;
            
            chunk.epoch = UINT64_MAX;
            markDirty(origin);
            ++mBuild;
        }
    }
    
    size_t visited = 0;
    while (indexed && mBuild && visited++ < mBuild && budget.available()) {
        if (mVisibility >= mBuild) mVisibility = 0;
        auto  origin = origins[mVisibility];
        auto& chunk  = subChunks.at(util::encodeSubChunkKey(origin));
        auto  stamp  = visibilityStamp(placement, verification, origin);
        bool current = chunk.epoch == verification.epoch && chunk.viewRevision == view.revision
                    && chunk.stamp == stamp && chunk.visibilityOrigin == placement.origin;
        if (chunk.ready && current) {
            ++mVisibility;
            continue;
        }
        if (chunk.ready) {
            chunk.ready  = false;
            chunk.cursor = 0;
        }
        if (!current) chunk.cursor = 0;
        if (chunk.cursor == 0) {
            chunk.epoch            = verification.epoch;
            chunk.viewRevision     = view.revision;
            chunk.stamp            = stamp;
            chunk.visibilityOrigin = placement.origin;
        }
        while (chunk.cursor < chunk.blocks.size() && budget.available()) {
            auto& entry = chunk.blocks[chunk.cursor++];
            bool  shown = visible(placement, verification, view, entry.pos);
            if (blockSource->setProjectionVisible(entry.pos, shown)) {
                write(entry.pos, shown ? *entry.block : blockSource->getBlock(entry.pos));
                markDirty(entry.pos);
            }
        }
        if (chunk.cursor < chunk.blocks.size()) break;
        chunk.cursor = 0;
        chunk.ready  = true;
        ++mVisibility;
    }
}
bool FakeWorld::ready(BlockPos origin) const {
    auto it = subChunks.find(util::encodeSubChunkKey(origin));
    return it != subChunks.end() && it->second.ready;
}
} 
