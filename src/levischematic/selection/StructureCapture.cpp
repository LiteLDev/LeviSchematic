#include "StructureCapture.h"
#include "VolumeMerge.h"
#include "levischematic/schematic/placement/VolumeIndex.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/Int64Tag.h"
#include "mc/deps/nbt/IntArrayTag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/legacy/ActorRuntimeID.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include "mc/world/phys/AABB.h"
#include <stdexcept>

namespace levischematic::selection {
namespace {
CompoundTag&       compound(CompoundTag& tag, char const* key) { return tag.at(key).get<CompoundTag>(); }
CompoundTag const& compound(CompoundTag const& tag, char const* key) { return tag.at(key).get<CompoundTag>(); }
ListTag&           list(CompoundTag& tag, char const* key) { return tag.at(key).get<ListTag>(); }
ListTag const&     list(CompoundTag const& tag, char const* key) { return tag.at(key).get<ListTag>(); }
ListTag            position(BlockPos const& value) { return ListTag{value.x, value.y, value.z}; }
} 
StructureCapture::StructureCapture(BlockPos origin, BlockPos size) : mOrigin(origin), mSize(size) {
    auto volume = placement::volumeSize({size.x, size.y, size.z});
    if (!volume) throw std::runtime_error("Selection exceeds supported index range.");
    mPrimary.assign(*volume, -1);
    mExtra.assign(*volume, -1);
}
StructureCapture::~StructureCapture() = default;
std::vector<uint64_t>
captureEntityIds(BlockSource& source, StructureTemplate const& filter, BlockPos origin, BlockPos size) {
    AABB box{
        Vec3{static_cast<float>(origin.x),                   static_cast<float>(origin.y), static_cast<float>(origin.z)},
        Vec3{
             static_cast<float>(int64_t{origin.x} + size.x),
             static_cast<float>(int64_t{origin.y} + size.y),
             static_cast<float>(int64_t{origin.z} + size.z)
        }
    };
    std::vector<uint64_t> result;
    
    for (auto const& actor : source.fetchEntities(nullptr, box, false, false))
        if (filter._allowReadActor(*actor)) result.push_back(actor->getRuntimeID().rawID);
    return result;
}
void StructureCapture::append(CompoundTag const& tile, BlockPos offset, std::span<uint64_t const> entityIds) {
    auto const& sizeTag = list(tile, "size");
    if (sizeTag.size() != 3) throw std::runtime_error("Invalid capture size.");
    BlockPos    size{sizeTag[0].get<IntTag>().data, sizeTag[1].get<IntTag>().data, sizeTag[2].get<IntTag>().data};
    auto const& source        = compound(tile, "structure");
    auto const& sourcePalette = compound(compound(source, "palette"), "default");
    if (!mTag) {
        mTag            = tile.clone();
        auto& structure = compound(*mTag, "structure");
        auto& palette   = compound(compound(structure, "palette"), "default");
        list(palette, "block_palette").clear();
        compound(palette, "block_position_data").mTags.clear();
        list(structure, "entities").clear();
    }
    auto&            target  = compound(*mTag, "structure");
    auto&            palette = compound(compound(target, "palette"), "default");
    auto&            blocks  = list(palette, "block_palette");
    std::vector<int> remap;
    for (auto const& block : list(sourcePalette, "block_palette")) {
        auto key            = block.get<CompoundTag>().toBinaryNbt();
        auto [it, inserted] = mPalette.try_emplace(std::move(key), static_cast<int>(blocks.size()));
        if (inserted) blocks.emplace_back(block->copy());
        remap.push_back(it->second);
    }
    auto relocate = [&](size_t index) {
        auto pos = placement::volumePosition(index, {size.x, size.y, size.z});
        return placement::volumeIndex(
            {pos[0] + offset.x, pos[1] + offset.y, pos[2] + offset.z},
            {mSize.x, mSize.y, mSize.z}
        );
    };
    auto const& layers = list(source, "block_indices");
    
    if (layers.empty() || layers.size() > 2) throw std::runtime_error("Invalid capture block layers.");
    for (size_t layer = 0; layer < layers.size(); ++layer) {
        auto&            output = layer == 0 ? mPrimary : mExtra;
        std::vector<int> input;
        if (layers[layer].getId() == Tag::IntArray) input = layers[layer].get<IntArrayTag>();
        else if (layers[layer].getId() == Tag::List) {
            auto const& indices = layers[layer].get<ListTag>();
            input.reserve(indices.size());
            for (auto const& index : indices) input.push_back(index.get<IntTag>().data);
        } else throw std::runtime_error("Invalid capture index layer type.");
        mergeBlockLayer(
            output,
            input,
            remap,
            {size.x, size.y, size.z},
            {mSize.x, mSize.y, mSize.z},
            {offset.x, offset.y, offset.z}
        );
    }
    for (auto const& [key, data] : compound(sourcePalette, "block_position_data").mTags)
        compound(palette, "block_position_data")[std::to_string(relocate(std::stoull(key)))] = data;
    auto const& entities = list(source, "entities");
    if (entities.size() != entityIds.size())
        throw std::runtime_error("Entity enumeration changed during native capture.");
    for (size_t i = 0; i < entities.size(); ++i) {
        auto const& entity = entities[i];
        if (entity.get<CompoundTag>().empty())
            throw std::runtime_error(
                "An entity cannot be serialized from this source. Disable entities to save blocks only."
            );
        if (mEntities.insert(entityIds[i]).second) list(target, "entities").emplace_back(entity->copy());
    }
}
std::unique_ptr<CompoundTag> StructureCapture::finish() {
    if (!mTag) throw std::runtime_error("No blocks were captured.");
    (*mTag)["size"]                   = position(mSize);
    (*mTag)["structure_world_origin"] = position(mOrigin);
    auto& structure                   = compound(*mTag, "structure");
    structure["block_indices"]        = ListTag{IntArrayTag(std::move(mPrimary)), IntArrayTag(std::move(mExtra))};
    return std::move(mTag);
}
} 
