#include "levischematic/schematic/convert/LitematicMetadata.h"
#include <stdexcept>

namespace levischematic::convert {
namespace {
nbt::Compound const& rootCompound(nbt::NamedRoot const& root) {
    auto const* compound = root.tag.as<nbt::Compound>();
    if (!compound) throw std::runtime_error("litematic: root is not a compound.");
    return *compound;
}
int64_t integer(nbt::Compound const& c, std::string_view key, int64_t fallback) {
    int64_t value = fallback;
    if (auto const* tag = c.find(key)) (void)tag->integer(value);
    return value;
}
std::string text(nbt::Compound const& c, std::string_view key) {
    if (auto const* tag = c.find(key))
        if (auto const* s = tag->as<std::string>()) return *s;
    return {};
}
} // namespace

LitematicMetadata readLitematicMetadata(nbt::NamedRoot const& root) {
    auto const&       compound = rootCompound(root);
    LitematicMetadata result;
    result.version      = static_cast<int32_t>(integer(compound, "Version", 0));
    result.subVersion   = static_cast<int32_t>(integer(compound, "SubVersion", 0));
    result.dataVersion  = static_cast<int32_t>(integer(compound, "MinecraftDataVersion", 0));
    auto const* metaTag = compound.find("Metadata");
    auto const* meta    = metaTag ? metaTag->as<nbt::Compound>() : nullptr;
    if (!meta) throw std::runtime_error("litematic: missing Metadata.");
    result.name         = text(*meta, "Name");
    result.author       = text(*meta, "Author");
    result.description  = text(*meta, "Description");
    result.timeCreated  = integer(*meta, "TimeCreated", 0);
    result.timeModified = integer(*meta, "TimeModified", 0);
    result.regionCount  = static_cast<int32_t>(integer(*meta, "RegionCount", 0));
    result.totalBlocks  = integer(*meta, "TotalBlocks", -1);
    result.totalVolume  = integer(*meta, "TotalVolume", -1);
    if (auto const* sizeTag = meta->find("EnclosingSize"))
        if (auto const* size = sizeTag->as<nbt::Compound>()) {
            result.enclosingSize = {
                static_cast<int>(integer(*size, "x", 0)),
                static_cast<int>(integer(*size, "y", 0)),
                static_cast<int>(integer(*size, "z", 0))
            };
        }
    if (result.regionCount == 0)
        if (auto const* regions = compound.find("Regions"))
            if (auto const* c = regions->as<nbt::Compound>())
                result.regionCount = static_cast<int32_t>(c->entries.size());
    return result;
}

void writeLitematicMetadata(
    nbt::NamedRoot&                   root,
    std::optional<std::string> const& name,
    std::optional<std::string> const& author,
    int64_t                           nowMilliseconds
) {
    auto* compound = root.tag.as<nbt::Compound>();
    if (!compound) throw std::runtime_error("litematic: root is not a compound.");
    auto* metaTag = compound->find("Metadata");
    auto* meta    = metaTag ? metaTag->as<nbt::Compound>() : nullptr;
    if (!meta) throw std::runtime_error("litematic: missing Metadata.");
    auto set = [&](char const* key, nbt::Tag value) {
        if (auto* tag = meta->find(key)) *tag = std::move(value);
        else meta->put(key, std::move(value));
    };
    if (name) set("Name", *name);
    if (author) set("Author", *author);
    set("TimeModified", nowMilliseconds);
}
} // namespace levischematic::convert
