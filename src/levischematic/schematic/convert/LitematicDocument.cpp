#include "levischematic/schematic/convert/LitematicDocument.h"
#include "levischematic/schematic/convert/LitematicBitArray.h"
#include "levischematic/schematic/placement/model/VolumeIndex.h"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace levischematic::convert {
namespace {
using nbt::Compound;
using nbt::List;
using nbt::Tag;
using nbt::TagType;

constexpr std::string_view Air = "minecraft:air";

void checkStop(std::stop_token const& stop) {
    if (stop.stop_requested()) throw std::runtime_error("Cancelled.");
}
Compound const& requireCompound(Compound const& parent, std::string_view key) {
    auto const* tag = parent.find(key);
    auto const* c   = tag ? tag->as<Compound>() : nullptr;
    if (!c) throw std::runtime_error("litematic: missing compound '" + std::string(key) + "'.");
    return *c;
}
int32_t requireInt(Compound const& parent, std::string_view key) {
    int64_t     value = 0;
    auto const* tag   = parent.find(key);
    if (!tag || !tag->integer(value) || value < INT32_MIN || value > INT32_MAX)
        throw std::runtime_error("litematic: missing integer '" + std::string(key) + "'.");
    return static_cast<int32_t>(value);
}
std::string optionalString(Compound const& parent, std::string_view key) {
    auto const* tag = parent.find(key);
    auto const* s   = tag ? tag->as<std::string>() : nullptr;
    return s ? *s : std::string{};
}
int64_t optionalLong(Compound const& parent, std::string_view key) {
    int64_t     value = 0;
    auto const* tag   = parent.find(key);
    if (tag) (void)tag->integer(value);
    return value;
}
std::array<int, 3> readVec(Compound const& parent, std::string_view key) {
    auto const& c = requireCompound(parent, key);
    return {requireInt(c, "x"), requireInt(c, "y"), requireInt(c, "z")};
}
Tag vec(std::array<int, 3> v) {
    Compound c;
    c.put("x", int32_t{v[0]});
    c.put("y", int32_t{v[1]});
    c.put("z", int32_t{v[2]});
    return c;
}
size_t listSize(Compound const& parent, std::string_view key) {
    auto const* tag = parent.find(key);
    auto const* l   = tag ? tag->as<List>() : nullptr;
    return l ? l->items.size() : 0;
}
Tag emptyList(TagType type) {
    List l;
    l.elementType = type;
    return l;
}

LitematicRegion decodeRegion(std::string name, Compound const& region, std::stop_token const& stop) {
    LitematicRegion result;
    result.name   = std::move(name);
    auto position = readVec(region, "Position");
    auto size     = readVec(region, "Size");
    for (int axis = 0; axis < 3; ++axis) {
        if (size[axis] == 0 || size[axis] == INT32_MIN) throw std::runtime_error("litematic: invalid region size.");
        int64_t min = size[axis] < 0 ? int64_t{position[axis]} + size[axis] + 1 : position[axis];
        if (min < INT32_MIN) throw std::runtime_error("litematic: region position overflow.");
        result.position[axis] = static_cast<int>(min);
        result.size[axis]     = std::abs(size[axis]);
    }
    auto volume = placement::volumeSize(result.size);
    if (!volume) throw std::runtime_error("litematic: region volume exceeds supported index range.");

    auto const* paletteTag = region.find("BlockStatePalette");
    auto const* palette    = paletteTag ? paletteTag->as<List>() : nullptr;
    if (!palette || palette->items.empty()) throw std::runtime_error("litematic: region has no block palette.");
    result.palette.reserve(palette->items.size());
    for (auto const& entry : palette->items) {
        auto const* c = entry.as<Compound>();
        if (!c) throw std::runtime_error("litematic: invalid palette entry.");
        std::vector<std::pair<std::string, std::string>> properties;
        if (auto const* p = c->find("Properties"))
            if (auto const* pc = p->as<Compound>())
                for (auto const& [key, value] : pc->entries)
                    if (auto const* s = value.as<std::string>()) properties.emplace_back(key, *s);
        auto blockName = optionalString(*c, "Name");
        if (blockName.empty()) throw std::runtime_error("litematic: palette entry has no Name.");
        result.palette.push_back(canonicalJavaState(blockName, std::move(properties)));
    }

    auto const* statesTag = region.find("BlockStates");
    auto const* states    = statesTag ? statesTag->as<std::vector<int64_t>>() : nullptr;
    if (!states) throw std::runtime_error("litematic: region has no BlockStates long array.");
    auto bits   = litematicBitsForPalette(result.palette.size());
    auto packed = unpackLitematicBits(*states, bits, *volume);
    checkStop(stop);

    auto [sx, sy, sz] = result.size;
    result.blocks.resize(*volume);
    size_t source = 0;
    for (int y = 0; y < sy; ++y) {
        checkStop(stop);
        for (int z = 0; z < sz; ++z)
            for (int x = 0; x < sx; ++x) {
                auto index = packed[source++];
                if (index >= result.palette.size()) throw std::runtime_error("litematic: palette index out of range.");
                result.blocks[placement::volumeIndex({x, y, z}, result.size)] = index;
            }
    }
    result.tileEntities = listSize(region, "TileEntities");
    result.entities     = listSize(region, "Entities");
    result.pendingTicks = listSize(region, "PendingBlockTicks") + listSize(region, "PendingFluidTicks");
    return result;
}

Tag encodeRegion(LitematicRegion const& region, size_t& nonAir) {
    auto volume = placement::volumeSize(region.size);
    if (!volume || region.blocks.size() != *volume) throw std::runtime_error("litematic: region block count mismatch.");
    std::vector<std::string>                  palette{std::string(Air)};
    std::unordered_map<std::string, uint32_t> lookup{
        {std::string(Air), 0u}
    };
    std::vector<uint32_t> remap(region.palette.size(), UINT32_MAX);
    for (size_t i = 0; i < region.palette.size(); ++i) {
        auto [it, inserted] = lookup.try_emplace(region.palette[i], static_cast<uint32_t>(palette.size()));
        if (inserted) palette.push_back(region.palette[i]);
        remap[i] = it->second;
    }
    auto [sx, sy, sz] = region.size;
    std::vector<uint32_t> values(*volume);
    size_t                target = 0;
    for (int y = 0; y < sy; ++y)
        for (int z = 0; z < sz; ++z)
            for (int x = 0; x < sx; ++x) {
                auto index = region.blocks[placement::volumeIndex({x, y, z}, region.size)];
                if (index >= remap.size()) throw std::runtime_error("litematic: palette index out of range.");
                auto mapped = remap[index];
                if (mapped != 0) ++nonAir;
                values[target++] = mapped;
            }

    List paletteList;
    paletteList.elementType = TagType::Compound;
    for (auto const& state : palette) {
        auto [name, properties] = splitJavaState(state);
        Compound entry;
        entry.put("Name", name);
        if (!properties.empty()) {
            Compound props;
            for (auto& [key, value] : properties) props.put(key, value);
            entry.put("Properties", std::move(props));
        }
        paletteList.items.emplace_back(std::move(entry));
    }
    Compound result;
    result.put("Position", vec(region.position));
    result.put("Size", vec(region.size));
    result.put("BlockStatePalette", std::move(paletteList));
    result.put("BlockStates", packLitematicBits(values, litematicBitsForPalette(palette.size())));
    result.put("TileEntities", emptyList(TagType::Compound));
    result.put("Entities", emptyList(TagType::Compound));
    result.put("PendingBlockTicks", emptyList(TagType::Compound));
    result.put("PendingFluidTicks", emptyList(TagType::Compound));
    return result;
}
}

std::string canonicalJavaState(std::string_view name, std::vector<std::pair<std::string, std::string>> properties) {
    std::string result =
        name.find(':') == std::string_view::npos ? "minecraft:" + std::string(name) : std::string(name);
    if (properties.empty()) return result;
    std::sort(properties.begin(), properties.end());
    result += '[';
    for (size_t i = 0; i < properties.size(); ++i) {
        if (i) result += ',';
        result += properties[i].first;
        result += '=';
        result += properties[i].second;
    }
    result += ']';
    return result;
}
std::pair<std::string, std::vector<std::pair<std::string, std::string>>> splitJavaState(std::string_view state) {
    std::pair<std::string, std::vector<std::pair<std::string, std::string>>> result;
    auto                                                                     open = state.find('[');
    result.first                                                                  = std::string(state.substr(0, open));
    if (open == std::string_view::npos) return result;
    auto body = state.substr(open + 1);
    if (!body.empty() && body.back() == ']') body.remove_suffix(1);
    while (!body.empty()) {
        auto comma = body.find(',');
        auto part  = body.substr(0, comma);
        auto equal = part.find('=');
        if (equal != std::string_view::npos)
            result.second.emplace_back(std::string(part.substr(0, equal)), std::string(part.substr(equal + 1)));
        if (comma == std::string_view::npos) break;
        body.remove_prefix(comma + 1);
    }
    return result;
}

LitematicDocument decodeLitematic(nbt::NamedRoot const& root, std::stop_token stop) {
    auto const* compound = root.tag.as<Compound>();
    if (!compound) throw std::runtime_error("litematic: root is not a compound.");
    LitematicDocument document;
    document.version = requireInt(*compound, "Version");
    if (document.version < 1 || document.version > LitematicVersion)
        throw std::runtime_error("litematic: unsupported schematic version " + std::to_string(document.version) + ".");
    document.subVersion  = static_cast<int32_t>(optionalLong(*compound, "SubVersion"));
    document.dataVersion = static_cast<int32_t>(optionalLong(*compound, "MinecraftDataVersion"));
    if (auto const* metaTag = compound->find("Metadata"))
        if (auto const* meta = metaTag->as<Compound>()) {
            document.name         = optionalString(*meta, "Name");
            document.author       = optionalString(*meta, "Author");
            document.description  = optionalString(*meta, "Description");
            document.timeCreated  = optionalLong(*meta, "TimeCreated");
            document.timeModified = optionalLong(*meta, "TimeModified");
        }
    auto const& regions = requireCompound(*compound, "Regions");
    for (auto const& [name, tag] : regions.entries) {
        checkStop(stop);
        auto const* region = tag.as<Compound>();
        if (!region) throw std::runtime_error("litematic: invalid region '" + name + "'.");
        document.regions.push_back(decodeRegion(name, *region, stop));
    }
    if (document.regions.empty()) throw std::runtime_error("litematic: file has no regions.");
    return document;
}

nbt::NamedRoot encodeLitematic(LitematicDocument const& document) {
    if (document.regions.empty()) throw std::runtime_error("litematic: nothing to write.");
    Compound                        regions;
    std::unordered_set<std::string> names;
    size_t                          nonAir = 0;
    uint64_t                        volume = 0;
    std::array<int64_t, 3>          min{INT64_MAX, INT64_MAX, INT64_MAX}, max{INT64_MIN, INT64_MIN, INT64_MIN};
    for (auto const& region : document.regions) {
        if (!names.insert(region.name).second) throw std::runtime_error("litematic: duplicate region name.");
        regions.put(region.name, encodeRegion(region, nonAir));
        volume += static_cast<uint64_t>(region.size[0]) * region.size[1] * region.size[2];
        for (int axis = 0; axis < 3; ++axis) {
            min[axis] = std::min<int64_t>(min[axis], region.position[axis]);
            max[axis] = std::max<int64_t>(max[axis], int64_t{region.position[axis]} + region.size[axis]);
        }
    }
    std::array<int, 3> enclosing{};
    for (int axis = 0; axis < 3; ++axis)
        enclosing[axis] = static_cast<int>(std::min<int64_t>(max[axis] - min[axis], INT32_MAX));
    auto clampInt = [](uint64_t v) { return static_cast<int32_t>(std::min<uint64_t>(v, INT32_MAX)); };

    Compound metadata;
    metadata.put("Name", document.name);
    metadata.put("Author", document.author);
    metadata.put("Description", document.description);
    metadata.put("RegionCount", static_cast<int32_t>(document.regions.size()));
    metadata.put("TotalVolume", clampInt(volume));
    metadata.put("TotalBlocks", clampInt(nonAir));
    metadata.put("TimeCreated", int64_t{document.timeCreated});
    metadata.put("TimeModified", int64_t{document.timeModified});
    metadata.put("EnclosingSize", vec(enclosing));

    Compound root;
    root.put("MinecraftDataVersion", int32_t{document.dataVersion});
    root.put("Version", int32_t{LitematicVersion});
    root.put("SubVersion", int32_t{LitematicSubVersion});
    root.put("Metadata", std::move(metadata));
    root.put("Regions", std::move(regions));
    return {std::string{}, std::move(root)};
}
}
