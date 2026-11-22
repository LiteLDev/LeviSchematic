#include "levischematic/schematic/convert/LitematicConverter.h"
#include "levischematic/schematic/convert/JavaNbt.h"
#include "levischematic/schematic/convert/LitematicDocument.h"
#include "levischematic/schematic/io/LeviSchematicFile.h"
#include "levischematic/schematic/io/StructureMerge.h"
#include "levischematic/schematic/placement/model/VolumeIndex.h"
#include "levischematic/shared/i18n/I18n.h"
#include "levischematic/shared/io/AtomicFile.h"
#include "ll/api/utils/CompressUtils.h"
#include "mc/deps/nbt/ByteTag.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/Int64Tag.h"
#include "mc/deps/nbt/IntArrayTag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/ShortTag.h"
#include "mc/deps/nbt/StringTag.h"
#include "mc/world/level/block/Block.h"
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace levischematic::convert {
namespace {
std::mutex                          gMappingMutex;
std::filesystem::path               gMappingPath;
std::shared_ptr<BlockMapping const> gMapping;
std::atomic<int32_t>                gBlockVersion{0};

std::string readBytes(std::filesystem::path const& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open " + path.filename().string() + ".");
    std::string bytes{std::istreambuf_iterator<char>(input), {}};
    if (input.bad()) throw std::runtime_error("Cannot read " + path.filename().string() + ".");
    return bytes;
}
void checkStop(std::stop_token const& stop) {
    if (stop.stop_requested()) throw std::runtime_error("Cancelled.");
}
void note(ConversionReport& report, MatchQuality quality, std::string const& state) {
    if (quality == MatchQuality::Fallback) ++report.fallbackStates;
    if (quality != MatchQuality::Unmapped) return;
    ++report.unmappedStates;
    if (report.unmappedSamples.size() < 5) report.unmappedSamples.push_back(state);
}

CompoundTag const& child(CompoundTag const& tag, char const* key) {
    if (!tag.contains(key, Tag::Compound)) throw std::runtime_error(std::string("mcstructure: missing ") + key + ".");
    return tag.at(key).get<CompoundTag>();
}
ListTag const& childList(CompoundTag const& tag, char const* key) {
    if (!tag.contains(key, Tag::List)) throw std::runtime_error(std::string("mcstructure: missing ") + key + ".");
    return tag.at(key).get<ListTag>();
}
std::vector<int> indexLayer(UniqueTagPtr const& layer) {
    if (layer.getId() == Tag::IntArray) return layer.get<IntArrayTag>();
    if (layer.getId() != Tag::List) throw std::runtime_error("mcstructure: invalid block index layer.");
    std::vector<int> result;
    for (auto const& index : layer.get<ListTag>()) result.push_back(index.get<IntTag>().data);
    return result;
}
BedrockBlockState readBedrockState(CompoundTag const& entry) {
    BedrockBlockState state;
    if (!entry.contains("name", Tag::String)) throw std::runtime_error("mcstructure: palette entry has no name.");
    state.name = entry.at("name").get<StringTag>();
    if (entry.contains("states", Tag::Compound))
        for (auto const& [key, value] : entry.at("states").get<CompoundTag>().mTags) {
            switch (value.getId()) {
            case Tag::Byte:
                state.states.emplace_back(key, value.get<ByteTag>().data != 0);
                break;
            case Tag::Short:
                state.states.emplace_back(key, static_cast<int32_t>(value.get<ShortTag>().data));
                break;
            case Tag::Int:
                state.states.emplace_back(key, static_cast<int32_t>(value.get<IntTag>().data));
                break;
            case Tag::Int64:
                state.states.emplace_back(key, static_cast<int32_t>(value.get<Int64Tag>().data));
                break;
            case Tag::String:
                state.states.emplace_back(key, std::string(value.get<StringTag>()));
                break;
            default:
                break;
            }
        }
    return state;
}
CompoundTag writeBedrockState(BedrockBlockState const& state, int32_t version) {
    CompoundTag states;
    for (auto const& [key, value] : state.states) {
        if (auto const* b = std::get_if<bool>(&value)) states[key] = ByteTag{*b ? 1 : 0};
        else if (auto const* i = std::get_if<int32_t>(&value)) states[key] = IntTag{*i};
        else states[key] = StringTag{std::get<std::string>(value)};
    }
    CompoundTag entry;
    entry["name"]    = StringTag{state.name};
    entry["states"]  = std::move(states);
    entry["version"] = IntTag{version};
    return entry;
}
bool isWater(BedrockBlockState const& state) {
    return state.name == "minecraft:water" || state.name == "minecraft:flowing_water";
}

std::unique_ptr<CompoundTag>
toStructure(LitematicRegion const& region, BlockMapping const& map, int32_t version, ConversionReport& report) {
    std::vector<BedrockBlockState>       palette;
    std::unordered_map<std::string, int> lookup;
    std::vector<int>                     primaryOf(region.palette.size());
    std::vector<bool>                    wetOf(region.palette.size());
    auto                                 intern = [&](BedrockBlockState const& state) {
        auto [it, inserted] = lookup.try_emplace(bedrockStateKey(state), static_cast<int>(palette.size()));
        if (inserted) palette.push_back(state);
        return it->second;
    };
    bool anyWet = false;
    for (size_t i = 0; i < region.palette.size(); ++i) {
        auto mapped = map.toBedrock(region.palette[i]);
        note(report, mapped.quality, region.palette[i]);
        primaryOf[i]  = intern(mapped.block);
        wetOf[i]      = mapped.waterlogged;
        anyWet       |= mapped.waterlogged;
    }
    int water = anyWet ? intern(map.toBedrock("minecraft:water[level=0]").block) : -1;

    std::vector<int> primary(region.blocks.size()), extra(region.blocks.size(), -1);
    for (size_t i = 0; i < region.blocks.size(); ++i) {
        auto index = region.blocks[i];
        primary[i] = primaryOf[index];
        if (wetOf[index]) extra[i] = water;
    }
    ListTag blocks;
    for (auto const& state : palette) blocks.emplace_back(writeBedrockState(state, version));

    CompoundTag defaultPalette;
    defaultPalette["block_palette"]       = std::move(blocks);
    defaultPalette["block_position_data"] = CompoundTag{};
    CompoundTag palettes;
    palettes["default"] = std::move(defaultPalette);
    CompoundTag structure;
    structure["block_indices"] = ListTag{IntArrayTag(std::move(primary)), IntArrayTag(std::move(extra))};
    structure["entities"]      = ListTag{};
    structure["palette"]       = std::move(palettes);

    auto tag                         = std::make_unique<CompoundTag>();
    (*tag)["format_version"]         = IntTag{2};
    (*tag)["size"]                   = ListTag{region.size[0], region.size[1], region.size[2]};
    (*tag)["structure"]              = std::move(structure);
    (*tag)["structure_world_origin"] = ListTag{0, 0, 0};
    return tag;
}

LitematicRegion fromStructure(StructureRegionRef const& source, BlockMapping const& map, ConversionReport& report) {
    auto const& tag     = *source.tag;
    auto const& sizeTag = childList(tag, "size");
    if (sizeTag.size() != 3) throw std::runtime_error("mcstructure: invalid size.");
    LitematicRegion region;
    region.name     = source.name;
    region.position = {source.offset.x, source.offset.y, source.offset.z};
    region.size     = {sizeTag[0].get<IntTag>().data, sizeTag[1].get<IntTag>().data, sizeTag[2].get<IntTag>().data};
    auto volume     = placement::volumeSize(region.size);
    if (!volume) throw std::runtime_error("mcstructure: invalid structure size.");

    auto const& structure = child(tag, "structure");
    auto const& layers    = childList(structure, "block_indices");
    if (layers.empty()) throw std::runtime_error("mcstructure: no block layers.");
    auto primary = indexLayer(layers[0]);
    auto extra   = layers.size() > 1 ? indexLayer(layers[1]) : std::vector<int>{};
    if (primary.size() != *volume || (!extra.empty() && extra.size() != *volume))
        throw std::runtime_error("mcstructure: block index count does not match size.");

    auto const&                    defaultPalette = child(child(structure, "palette"), "default");
    std::vector<BedrockBlockState> bedrock;
    for (auto const& entry : childList(defaultPalette, "block_palette"))
        bedrock.push_back(readBedrockState(entry.get<CompoundTag>()));
    if (defaultPalette.contains("block_position_data", Tag::Compound))
        report.droppedBlockEntities += defaultPalette.at("block_position_data").get<CompoundTag>().size();
    if (structure.contains("entities", Tag::List))
        report.droppedEntities += structure.at("entities").get<ListTag>().size();

    region.palette.push_back("minecraft:air");
    std::map<std::pair<int, bool>, uint32_t> lookup;
    region.blocks.resize(*volume);
    for (size_t i = 0; i < *volume; ++i) {
        int index = primary[i];
        if (index < -1 || index >= static_cast<int>(bedrock.size()))
            throw std::runtime_error("mcstructure: palette index out of range.");
        if (index < 0) continue;
        bool wet            = !extra.empty() && extra[i] >= 0 && extra[i] < static_cast<int>(bedrock.size())
                           && isWater(bedrock[static_cast<size_t>(extra[i])]);
        auto [it, inserted] = lookup.try_emplace({index, wet}, 0u);
        if (inserted) {
            auto mapped = map.toJava(bedrock[static_cast<size_t>(index)], wet);
            note(report, mapped.quality, bedrock[static_cast<size_t>(index)].name);
            it->second = static_cast<uint32_t>(region.palette.size());
            region.palette.push_back(std::move(mapped.state));
        }
        region.blocks[i] = it->second;
    }
    return region;
}

struct SourceData {
    std::string                  name;
    std::vector<StructureRegion> regions;
};
SourceData readSource(std::filesystem::path const& path, std::stop_token const& stop, ConversionReport& report) {
    SourceData data;
    data.name = path.stem().string();
    if (isLitematicPath(path)) data.regions = readLitematic(path, stop, report);
    else if (schematic::isLeviSchematicPath(path)) {
        auto document = schematic::readLeviSchematic(path, stop);
        data.name     = document.name;
        for (auto& r : document.regions) data.regions.push_back({r.name, r.offset, std::move(r.tag)});
    } else if (path.extension() == ".mcstructure") data.regions.push_back({data.name, {}, readMcstructureFile(path)});
    else throw std::runtime_error("Unsupported source format: " + path.filename().string());
    return data;
}
} // namespace

std::string ConversionReport::summary() const {
    std::vector<std::string> parts;
    auto                     add = [&](size_t n, char const* key) {
        if (n) parts.push_back(tr(key, n));
    };
    add(mergedRegions, "convert.merged_regions");
    add(unmappedStates, "convert.unmapped_states");
    add(fallbackStates, "convert.fallback_states");
    add(droppedBlockEntities, "convert.dropped_block_entities");
    add(droppedEntities, "convert.dropped_entities");
    add(droppedTicks, "convert.dropped_ticks");
    if (parts.empty()) return {};
    std::string list;
    for (auto const& part : parts) list += (list.empty() ? "" : tr("common.list_separator")) + part;
    std::string samples;
    for (auto const& s : unmappedSamples) samples += " " + s;
    return samples.empty() ? tr("convert.summary", list) : tr("convert.summary_samples", list, samples);
}

void setBlockMappingPath(std::filesystem::path path) {
    std::scoped_lock lock(gMappingMutex);
    gMappingPath = std::move(path);
    gMapping.reset();
}
std::shared_ptr<BlockMapping const> blockMapping() {
    std::scoped_lock lock(gMappingMutex);
    if (!gMapping) {
        if (gMappingPath.empty()) throw std::runtime_error("Block mapping path is not configured.");
        if (!std::filesystem::exists(gMappingPath))
            throw std::runtime_error("Block mapping file is missing: " + gMappingPath.string());
        gMapping = std::make_shared<BlockMapping const>(BlockMapping::parse(readBytes(gMappingPath)));
    }
    return gMapping;
}
void captureBedrockBlockVersion() {
    auto block = Block::tryGetFromRegistry(HashedString{"minecraft:stone"});
    if (!block) return;
    CompoundTag const& id = *block->mSerializationId;
    if (id.contains("version", Tag::Int)) gBlockVersion = id.at("version").get<IntTag>().data;
}
bool isLitematicPath(std::filesystem::path const& path) { return path.extension() == ".litematic"; }
std::unique_ptr<CompoundTag> readMcstructureFile(std::filesystem::path const& path) {
    auto tag = CompoundTag::fromBinaryNbt(readBytes(path), true);
    if (!tag) throw std::runtime_error("Cannot parse " + path.filename().string() + " as mcstructure.");
    return std::make_unique<CompoundTag>(std::move(*tag));
}

std::vector<StructureRegion>
readLitematic(std::filesystem::path const& path, std::stop_token stop, ConversionReport& report) {
    auto version = gBlockVersion.load();
    if (version == 0) throw std::runtime_error("Block registry is not ready; join a world first.");
    auto map   = blockMapping();
    auto bytes = readBytes(path);
    checkStop(stop);
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0x1F
        && static_cast<unsigned char>(bytes[1]) == 0x8B) {
        auto inflated = ll::utils::compress_utils::decompress(bytes, ll::utils::compress_utils::CompressFormat::Gzip);
        if (!inflated) throw std::runtime_error("Cannot decompress litematic: " + inflated.error().message());
        bytes = std::move(*inflated);
    }
    auto               document = decodeLitematic(nbt::readBigEndian(bytes), stop);
    std::array<int, 3> min{INT32_MAX, INT32_MAX, INT32_MAX};
    for (auto const& region : document.regions)
        for (int axis = 0; axis < 3; ++axis) min[axis] = std::min(min[axis], region.position[axis]);
    std::vector<StructureRegion> result;
    for (auto const& region : document.regions) {
        checkStop(stop);
        std::array<int64_t, 3> offset{};
        for (int axis = 0; axis < 3; ++axis) offset[axis] = int64_t{region.position[axis]} - min[axis];
        if (offset[0] > INT32_MAX || offset[1] > INT32_MAX || offset[2] > INT32_MAX)
            throw std::runtime_error("litematic: region offset overflow.");
        report.droppedBlockEntities += region.tileEntities;
        report.droppedEntities      += region.entities;
        report.droppedTicks         += region.pendingTicks;
        result.push_back({
            region.name,
            {static_cast<int>(offset[0]), static_cast<int>(offset[1]), static_cast<int>(offset[2])},
            toStructure(region, *map, version, report)
        });
    }
    return result;
}

std::string
encodeLitematicFile(std::span<StructureRegionRef const> regions, std::string const& name, ConversionReport& report) {
    auto              map = blockMapping();
    LitematicDocument document;
    document.dataVersion = map->javaDataVersion();
    document.name        = name;
    document.author      = "LeviSchematic";
    document.timeCreated = document.timeModified =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    std::unordered_set<std::string> used;
    for (auto const& source : regions) {
        auto region = fromStructure(source, *map, report);
        auto base   = region.name.empty() ? std::string("Region") : region.name;
        region.name = base;
        for (int n = 2; !used.insert(region.name).second; ++n) region.name = base + " (" + std::to_string(n) + ")";
        document.regions.push_back(std::move(region));
    }
    auto compressed = ll::utils::compress_utils::compress(
        nbt::writeBigEndian(convert::encodeLitematic(document)),
        ll::utils::compress_utils::CompressFormat::Gzip
    );
    if (!compressed) throw std::runtime_error("Cannot compress litematic: " + compressed.error().message());
    return std::move(*compressed);
}

void convertFile(
    std::filesystem::path const& source,
    std::filesystem::path const& target,
    bool                         overwrite,
    std::stop_token              stop,
    ConversionReport&            report
) {
    std::error_code ec;
    if (std::filesystem::equivalent(source, target, ec))
        throw std::runtime_error("Source and target are the same file.");
    if (!overwrite && std::filesystem::exists(target)) throw std::runtime_error("Target exists; confirm overwrite.");
    auto data = readSource(source, stop, report);
    checkStop(stop);
    if (isLitematicPath(target)) {
        std::vector<StructureRegionRef> refs;
        for (auto const& r : data.regions) refs.push_back({r.name, r.offset, r.tag.get()});
        util::writeAtomic(target, encodeLitematicFile(refs, target.stem().string(), report), overwrite);
    } else if (schematic::isLeviSchematicPath(target)) {
        std::vector<schematic::LeviSchematicRegionRef> refs;
        for (size_t i = 0; i < data.regions.size(); ++i) {
            auto const& r = data.regions[i];
            refs.push_back({std::to_string(i), r.name, r.offset, r.tag.get()});
        }
        schematic::LeviSchematicWriteInfo info;
        info.name = data.name;
        util::writeAtomic(target, schematic::encodeLeviSchematicFile(refs, info), overwrite);
    } else if (target.extension() == ".mcstructure") {
        if (data.regions.size() == 1) util::writeAtomic(target, data.regions.front().tag->toBinaryNbt(), overwrite);
        else {
            std::vector<schematic::MergeRegion> regions;
            for (auto const& r : data.regions) regions.push_back({r.offset, r.tag.get()});
            util::writeAtomic(target, schematic::mergeStructures(regions, {})->toBinaryNbt(), overwrite);
            report.mergedRegions = data.regions.size();
        }
    } else throw std::runtime_error("Unsupported target format: " + target.filename().string());
}
} // namespace levischematic::convert
