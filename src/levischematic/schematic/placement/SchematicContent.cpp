#include "SchematicContent.h"
#include "levischematic/schematic/FakeWorld.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/IntArrayTag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/world/level/block/states/VanillaBlockStateTransformUtils.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include <stdexcept>

namespace levischematic::placement {
namespace {
ListTag indexList(std::vector<int> const& values) {
    ListTag result;
    for (int value : values) result.emplace_back(IntTag{value});
    return result;
}
} 
Block const* localBlock(PlacementInstance const& p, Block const* block) {
    if (!block || block->isAir()) return block;
    using R      = PlacementInstance::Rotation;
    auto inverse = static_cast<R>((4 - static_cast<int>(p.rotation)) % 4);
    block        = VanillaBlockStateTransformUtils::transformBlock(
        *block,
        render::toMinecraftRotation(inverse),
        render::toMinecraftMirror(PlacementInstance::Mirror::NONE)
    );
    return VanillaBlockStateTransformUtils::transformBlock(
        *block,
        render::toMinecraftRotation(R::NONE),
        render::toMinecraftMirror(p.mirror)
    );
}
std::unique_ptr<CompoundTag> serializeContent(SchematicAsset const& asset) {
    if (!asset.structure) throw std::runtime_error("Resource has no structure data.");
    auto tag = asset.structure->save();
    if (!tag) throw std::runtime_error("Cannot serialize structure.");
    auto&   structure = tag->at("structure").get<CompoundTag>();
    auto&   palette   = structure.at("palette").get<CompoundTag>().at("default").get<CompoundTag>();
    ListTag blocks;
    for (auto const* block : asset.palette) {
        if (!block) throw std::runtime_error("Cannot save unresolved palette entries.");
        blocks.emplace_back(block->mSerializationId->clone());
    }
    palette["block_palette"] = std::move(blocks);
    auto& layers             = structure.at("block_indices").get<ListTag>();
    if (layers[0].getId() == Tag::IntArray) layers[0] = IntArrayTag(asset.indices);
    else layers[0] = indexList(asset.indices);
    if (layers.size() > 1 && !asset.editedCells.empty()) {
        std::vector<int> extra;
        if (layers[1].getId() == Tag::IntArray) extra = layers[1].get<IntArrayTag>();
        else
            for (auto const& n : layers[1].get<ListTag>()) extra.push_back(n.get<IntTag>().data);
        for (auto index : asset.editedCells)
            if (index < extra.size()) extra[index] = -1;
        if (layers[1].getId() == Tag::IntArray) layers[1] = IntArrayTag(extra);
        else layers[1] = indexList(extra);
    }
    auto& data = palette.at("block_position_data").get<CompoundTag>();
    for (auto index : asset.editedCells) data.mTags.erase(std::to_string(index));
    return tag;
}
} 
