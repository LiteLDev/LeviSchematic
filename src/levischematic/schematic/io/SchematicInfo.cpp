#include "levischematic/schematic/io/SchematicInfo.h"
#include "levischematic/schematic/convert/LitematicConverter.h"
#include "levischematic/schematic/io/LeviSchematicFile.h"
#include "levischematic/schematic/io/SchematicFiles.h"
#include "levischematic/shared/i18n/I18n.h"
#include "levischematic/shared/io/AtomicFile.h"
#include "ll/api/utils/CompressUtils.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/IntArrayTag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/StringTag.h"
#include <chrono>
#include <fstream>

namespace levischematic::schematic {
namespace {
std::string readBytes(std::filesystem::path const& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open file.");
    std::string bytes{std::istreambuf_iterator<char>(input), {}};
    if (input.bad()) throw std::runtime_error("Cannot read file.");
    return bytes;
}
std::string inflate(std::string bytes) {
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0x1F
        && static_cast<unsigned char>(bytes[1]) == 0x8B) {
        auto inflated = ll::utils::compress_utils::decompress(bytes, ll::utils::compress_utils::CompressFormat::Gzip);
        if (!inflated) throw std::runtime_error("Cannot decompress: " + inflated.error().message());
        return std::move(*inflated);
    }
    return bytes;
}
} // namespace
StructureStats structureStats(CompoundTag const& tag) {
    StructureStats info;
    if (tag.contains("size", Tag::List)) {
        auto const& size = tag.at("size").get<ListTag>();
        for (size_t i = 0; i < 3 && i < size.size(); ++i) info.size[i] = size[i].get<IntTag>().data;
    }
    info.formatVersion = tag.contains("format_version", Tag::Int) ? tag.at("format_version").get<IntTag>().data : 0;
    if (!tag.contains("structure", Tag::Compound)) return info;
    auto const& structure = tag.at("structure").get<CompoundTag>();
    if (!structure.contains("block_indices", Tag::List) || !structure.contains("palette", Tag::Compound)) return info;
    auto const& palette = structure.at("palette").get<CompoundTag>();
    if (!palette.contains("default", Tag::Compound)) return info;
    auto const& def = palette.at("default").get<CompoundTag>();
    if (!def.contains("block_palette", Tag::List)) return info;
    std::vector<bool> air;
    for (auto const& entry : def.at("block_palette").get<ListTag>()) {
        auto const& c    = entry.get<CompoundTag>();
        std::string name = c.contains("name", Tag::String) ? std::string(c.at("name").get<StringTag>()) : "";
        air.push_back(name == "minecraft:air" || name == "minecraft:structure_void");
    }
    auto const& layers = structure.at("block_indices").get<ListTag>();
    if (layers.empty()) return info;
    int64_t count = 0;
    auto    add   = [&](int index) {
        if (index >= 0 && index < static_cast<int>(air.size()) && !air[index]) ++count;
    };
    if (layers[0].getId() == Tag::IntArray) {
        std::vector<int> indices = layers[0].get<IntArrayTag>();
        for (int index : indices) add(index);
    } else if (layers[0].getId() == Tag::List)
        for (auto const& index : layers[0].get<ListTag>()) add(index.get<IntTag>().data);
    info.blocks = count;
    return info;
}

SchematicInfo readSchematicInfo(std::filesystem::path const& path) {
    SchematicInfo info;
    try {
        auto suffix = fileSuffix(path);
        info.name   = utf8(path.stem());
        if (suffix == ".litematic") {
            info.format    = "litematic";
            auto root      = convert::nbt::readBigEndian(inflate(readBytes(path)));
            auto meta      = convert::readLitematicMetadata(root);
            info.name      = meta.name;
            info.size      = meta.enclosingSize;
            info.blocks    = meta.totalBlocks;
            info.volume    = meta.totalVolume;
            info.regions   = meta.regionCount;
            info.litematic = std::move(meta);
        } else if (suffix == ".mcstructure") {
            info.format        = "mcstructure";
            info.regions       = 1;
            auto stats         = structureStats(*convert::readMcstructureFile(path));
            info.size          = stats.size;
            info.blocks        = stats.blocks;
            info.formatVersion = stats.formatVersion;
            info.volume        = int64_t{info.size[0]} * info.size[1] * info.size[2];
        } else if (suffix == LeviSchematicExtension) {
            info.format  = "levischematic";
            auto  header = readLeviSchematicHeader(path);
            auto& meta   = header.metadata;
            if (!meta.name.empty()) info.name = meta.name;
            info.size          = meta.enclosingSize;
            info.blocks        = meta.totalBlocks;
            info.volume        = meta.totalVolume;
            info.regions       = meta.regionCount;
            info.formatVersion = meta.dataVersion;
            info.metadata      = std::move(meta);
        } else throw std::runtime_error("Unsupported file type.");
        info.ok = true;
    } catch (std::exception const& e) {
        info.error = e.what();
    }
    return info;
}

std::optional<SchematicInfo> SchematicInfoCache::get(std::filesystem::path const& path) {
    std::error_code ec;
    auto            stamp = std::filesystem::last_write_time(path, ec);
    std::lock_guard lock(mMutex);
    auto            it = mEntries.find(path);
    if (it == mEntries.end() || it->second.stamp != stamp) {
        if (mEntries.size() > 256) mEntries.clear();
        auto future    = std::async(std::launch::async, [path] { return readSchematicInfo(path); }).share();
        mEntries[path] = {stamp, future};
        return {};
    }
    auto& value = it->second.value;
    if (value.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return {};
    return value.get();
}
void SchematicInfoCache::clear() {
    std::lock_guard lock(mMutex);
    mEntries.clear();
}

std::string editLitematicMetadata(
    std::filesystem::path const&      path,
    std::optional<std::string> const& name,
    std::optional<std::string> const& author
) {
    try {
        if (fileSuffix(path) == LeviSchematicExtension) return editLeviSchematicMetadata(path, name, author);
        if (fileSuffix(path) != ".litematic") return tr("error.metadata_unsupported");
        auto root = convert::nbt::readBigEndian(inflate(readBytes(path)));
        auto now =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        convert::writeLitematicMetadata(root, name, author, now);
        auto compressed = ll::utils::compress_utils::compress(
            convert::nbt::writeBigEndian(root),
            ll::utils::compress_utils::CompressFormat::Gzip
        );
        if (!compressed) return "Cannot compress litematic: " + compressed.error().message();
        util::writeAtomic(path, *compressed, true);
        return {};
    } catch (std::exception const& e) {
        return e.what();
    }
}
} // namespace levischematic::schematic
