#include "levischematic/schematic/io/StructureMerge.h"
#include "levischematic/schematic/placement/model/VolumeIndex.h"
#include "levischematic/selection/model/VolumeMerge.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/FloatTag.h"
#include "mc/deps/nbt/IntArrayTag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/deps/nbt/ListTag.h"
#include <stdexcept>
#include <unordered_map>

namespace levischematic::schematic {
namespace {
CompoundTag const& compound(CompoundTag const& tag, char const* key) {
    if (!tag.contains(key, Tag::Compound)) throw std::runtime_error(std::string("mcstructure: missing ") + key);
    return tag.at(key).get<CompoundTag>();
}
ListTag const& list(CompoundTag const& tag, char const* key) {
    if (!tag.contains(key, Tag::List)) throw std::runtime_error(std::string("mcstructure: missing ") + key);
    return tag.at(key).get<ListTag>();
}
std::array<int, 3> ints(CompoundTag const& tag, char const* key) {
    std::array<int, 3> result{};
    if (!tag.contains(key, Tag::List)) return result;
    auto const& values = tag.at(key).get<ListTag>();
    for (size_t i = 0; i < 3 && i < values.size(); ++i) result[i] = values[i].get<IntTag>().data;
    return result;
}
template <class Variant>
std::vector<int> layer(Variant const& tag) {
    if (tag.getId() == Tag::IntArray) return tag.template get<IntArrayTag>();
    std::vector<int> result;
    if (tag.getId() != Tag::List) throw std::runtime_error("mcstructure: invalid index layer.");
    for (auto const& index : tag.template get<ListTag>()) result.push_back(index.template get<IntTag>().data);
    return result;
}
void shiftBlockEntity(CompoundTag& data, std::array<int, 3> shift) {
    if (!data.contains("block_entity_data", Tag::Compound)) return;
    auto&       entity = data.at("block_entity_data").get<CompoundTag>();
    char const* keys[] = {"x", "y", "z"};
    for (int axis = 0; axis < 3; ++axis)
        if (entity.contains(keys[axis], Tag::Int)) entity.at(keys[axis]).get<IntTag>().data += shift[axis];
}
void shiftEntity(CompoundTag& entity, std::array<int, 3> shift) {
    if (!entity.contains("Pos", Tag::List)) return;
    auto& pos = entity.at("Pos").get<ListTag>();
    for (size_t axis = 0; axis < 3 && axis < pos.size(); ++axis)
        if (pos[axis].getId() == Tag::Float) pos[axis].get<FloatTag>().data += static_cast<float>(shift[axis]);
}
} // namespace

std::unique_ptr<CompoundTag> mergeStructures(std::span<MergeRegion const> regions, BlockPos worldOrigin) {
    if (regions.empty()) throw std::runtime_error("Nothing to merge.");
    std::vector<std::array<int, 3>> offsets, sizes;
    for (auto const& r : regions) {
        if (!r.tag) throw std::runtime_error("Missing region structure.");
        offsets.push_back({r.offset.x, r.offset.y, r.offset.z});
        sizes.push_back(ints(*r.tag, "size"));
    }
    auto extent = mergedExtent(offsets, sizes);
    if (!extent) throw std::runtime_error("Merged structure exceeds supported size.");
    auto             volume = *placement::volumeSize(*extent);
    std::vector<int> primary(volume, -1), extra(volume, -1);

    auto  result    = regions.front().tag->clone();
    auto& structure = result->at("structure").get<CompoundTag>();
    auto& palette   = structure.at("palette").get<CompoundTag>().at("default").get<CompoundTag>();
    auto& blocks    = palette.at("block_palette").get<ListTag>();
    blocks.clear();
    CompoundTag                          positionData;
    ListTag                              entities;
    std::unordered_map<std::string, int> keys;

    for (size_t r = 0; r < regions.size(); ++r) {
        auto const&      source        = compound(*regions[r].tag, "structure");
        auto const&      sourcePalette = compound(compound(source, "palette"), "default");
        std::vector<int> remap;
        for (auto const& block : list(sourcePalette, "block_palette")) {
            auto [it, inserted] =
                keys.try_emplace(block.get<CompoundTag>().toBinaryNbt(), static_cast<int>(blocks.size()));
            if (inserted) blocks.emplace_back(block->copy());
            remap.push_back(it->second);
        }
        auto const& layers = list(source, "block_indices");
        if (layers.empty() || layers.size() > 2) throw std::runtime_error("mcstructure: invalid block layers.");
        for (size_t l = 0; l < layers.size(); ++l)
            selection::mergeBlockLayer(
                l == 0 ? primary : extra,
                layer(layers[l]),
                remap,
                sizes[r],
                *extent,
                offsets[r]
            );
        auto               regionOrigin = ints(*regions[r].tag, "structure_world_origin");
        std::array<int, 3> shift{
            worldOrigin.x + offsets[r][0] - regionOrigin[0],
            worldOrigin.y + offsets[r][1] - regionOrigin[1],
            worldOrigin.z + offsets[r][2] - regionOrigin[2]
        };
        if (sourcePalette.contains("block_position_data", Tag::Compound))
            for (auto const& [key, data] : sourcePalette.at("block_position_data").get<CompoundTag>().mTags) {
                auto pos   = placement::volumePosition(std::stoull(key), sizes[r]);
                auto index = placement::volumeIndex(
                    {pos[0] + offsets[r][0], pos[1] + offsets[r][1], pos[2] + offsets[r][2]},
                    *extent
                );
                auto copy = data.get<CompoundTag>().clone();
                shiftBlockEntity(*copy, shift);
                positionData[std::to_string(index)] = std::move(*copy);
            }
        if (source.contains("entities", Tag::List))
            for (auto const& entity : source.at("entities").get<ListTag>()) {
                auto copy = entity.get<CompoundTag>().clone();
                shiftEntity(*copy, shift);
                entities.emplace_back(std::move(*copy));
            }
    }
    palette["block_position_data"]      = std::move(positionData);
    structure["entities"]               = std::move(entities);
    structure["block_indices"]          = ListTag{IntArrayTag(std::move(primary)), IntArrayTag(std::move(extra))};
    (*result)["size"]                   = ListTag{(*extent)[0], (*extent)[1], (*extent)[2]};
    (*result)["structure_world_origin"] = ListTag{worldOrigin.x, worldOrigin.y, worldOrigin.z};
    return result;
}
} // namespace levischematic::schematic
