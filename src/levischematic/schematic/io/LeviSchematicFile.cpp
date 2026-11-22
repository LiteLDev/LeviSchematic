#include "levischematic/schematic/io/LeviSchematicFile.h"
#include "levischematic/schematic/io/LeviSchematicContainer.h"
#include "levischematic/schematic/io/SchematicInfo.h"
#include "levischematic/shared/io/AtomicFile.h"
#include "ll/api/utils/CompressUtils.h"
#include "mc/deps/nbt/CompoundTag.h"
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

namespace levischematic::schematic {
namespace {
namespace cu = ll::utils::compress_utils;
int64_t nowMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}
std::string readAll(std::filesystem::path const& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open file.");
    std::string bytes{std::istreambuf_iterator<char>(input), {}};
    if (input.bad()) throw std::runtime_error("Cannot read file.");
    return bytes;
}
std::array<int, 3> triple(nlohmann::json const& value) {
    if (!value.is_array() || value.size() != 3) return {};
    return value.get<std::array<int, 3>>();
}
} // namespace

bool isLeviSchematicPath(std::filesystem::path const& path) { return path.extension() == LeviSchematicExtension; }

std::string
encodeLeviSchematicFile(std::span<LeviSchematicRegionRef const> regions, LeviSchematicWriteInfo const& info) {
    if (regions.empty()) throw std::runtime_error("Nothing to save.");
    std::vector<container::Entry>   entries;
    auto                            list = nlohmann::json::array();
    std::unordered_set<std::string> ids;
    std::array<int64_t, 3>          extent{};
    int64_t                         blocks = 0, volume = 0;
    int                             formatVersion = 0;
    for (size_t i = 0; i < regions.size(); ++i) {
        auto const& r = regions[i];
        if (!r.tag) throw std::runtime_error("Missing region structure.");
        auto id = r.id.empty() ? std::to_string(i) : r.id;
        if (!ids.insert(id).second) throw std::runtime_error("Duplicate region ID.");
        auto stats    = structureStats(*r.tag);
        formatVersion = std::max(formatVersion, stats.formatVersion);
        if (stats.blocks > 0) blocks += stats.blocks;
        volume          += int64_t{stats.size[0]} * stats.size[1] * stats.size[2];
        extent[0]        = std::max(extent[0], int64_t{r.offset.x} + stats.size[0]);
        extent[1]        = std::max(extent[1], int64_t{r.offset.y} + stats.size[1]);
        extent[2]        = std::max(extent[2], int64_t{r.offset.z} + stats.size[2]);
        auto raw         = r.tag->toBinaryNbt();
        auto compressed  = cu::compress(raw, cu::CompressFormat::Gzip);
        if (!compressed) throw std::runtime_error("Cannot compress region: " + compressed.error().message());
        entries.push_back({std::move(*compressed), raw.size(), container::crc32(raw), "gzip"});
        list.push_back({
            {"id",     id                                  },
            {"name",   r.name                              },
            {"offset", {r.offset.x, r.offset.y, r.offset.z}},
            {"size",   stats.size                          },
            {"blocks", stats.blocks                        },
            {"entry",  i                                   }
        });
    }
    auto           now = nowMilliseconds();
    nlohmann::json header{
        {"metadata",
         {{"name", info.name},
          {"author", info.author},
          {"description", info.description},
          {"timeCreated", info.timeCreated ? info.timeCreated : now},
          {"timeModified", now},
          {"regionCount", regions.size()},
          {"totalBlocks", blocks},
          {"totalVolume", volume},
          {"enclosingSize", extent},
          {"generator", "LeviSchematic"},
          {"formatVersion", formatVersion}}},
        {"origin",   info.origin           },
        {"regions",  std::move(list)       }
    };
    return container::encode(std::move(header), entries);
}

LeviSchematicHeaderInfo readLeviSchematicHeader(std::filesystem::path const& path) {
    auto                    header = container::readHeader(path);
    LeviSchematicHeaderInfo result;
    auto const&             meta = header.at("metadata");
    auto&                   m    = result.metadata;
    m.name                       = meta.value("name", std::string{});
    m.author                     = meta.value("author", std::string{});
    m.description                = meta.value("description", std::string{});
    m.timeCreated                = meta.value("timeCreated", int64_t{0});
    m.timeModified               = meta.value("timeModified", int64_t{0});
    m.regionCount                = meta.value("regionCount", 0);
    m.totalBlocks                = meta.value("totalBlocks", int64_t{-1});
    m.totalVolume                = meta.value("totalVolume", int64_t{-1});
    m.enclosingSize              = triple(meta.value("enclosingSize", nlohmann::json{}));
    m.version                    = header.value("version", 0);
    m.dataVersion                = meta.value("formatVersion", 0);
    result.origin                = triple(header.value("origin", nlohmann::json{}));
    for (auto const& r : header.value("regions", nlohmann::json::array())) {
        auto offset = triple(r.value("offset", nlohmann::json{}));
        result.regions.push_back({
            r.value("id", std::string{}
             ),
            r.value("name", std::string{}
             ),
            {offset[0], offset[1], offset[2]},
            triple(r.value("size", nlohmann::json{}
             )),
            r.value("blocks", int64_t{-1}
             )
        });
    }
    if (m.regionCount == 0) m.regionCount = static_cast<int32_t>(result.regions.size());
    return result;
}

LeviSchematicDocument readLeviSchematic(std::filesystem::path const& path, std::stop_token stop) {
    auto                  file   = readAll(path);
    auto                  header = container::decodeHeader(file);
    LeviSchematicDocument result;
    result.name = header.at("metadata").value("name", std::string{});
    if (result.name.empty()) result.name = path.stem().string();
    std::unordered_set<std::string> ids;
    auto const&                     entries = header.at("entries");
    for (auto const& r : header.at("regions")) {
        if (stop.stop_requested()) throw std::runtime_error("Cancelled.");
        auto id = r.at("id").get<std::string>();
        if (id.empty() || !ids.insert(id).second) throw std::runtime_error("Duplicate/empty region ID.");
        auto index = r.at("entry").get<size_t>();
        if (index >= entries.size()) throw std::runtime_error("Region references a missing entry.");
        auto const& entry = entries[index];
        auto        raw   = entry.at("rawLength").get<uint64_t>();
        if (entry.value("compression", std::string{}) != "gzip") throw std::runtime_error("Unsupported compression.");
        auto inflated = cu::decompress(container::entryBytes(file, header, index), cu::CompressFormat::Gzip, raw);
        if (!inflated) throw std::runtime_error("Cannot decompress region: " + inflated.error().message());
        if (inflated->size() != raw || container::crc32(*inflated) != entry.at("crc32").get<uint32_t>())
            throw std::runtime_error("Region data is corrupted (CRC mismatch).");
        auto tag = CompoundTag::fromBinaryNbt(*inflated, true);
        if (!tag) throw std::runtime_error("Cannot parse region structure.");
        auto offset = triple(r.at("offset"));
        for (int v : offset)
            if (v < 0) throw std::runtime_error("Invalid region offset.");
        result.regions.push_back({
            id,
            r.value("name", id),
            {offset[0], offset[1], offset[2]},
            std::make_unique<CompoundTag>(std::move(*tag))
        });
    }
    if (result.regions.empty()) throw std::runtime_error("File has no regions.");
    return result;
}

std::string editLeviSchematicMetadata(
    std::filesystem::path const&      path,
    std::optional<std::string> const& name,
    std::optional<std::string> const& author
) {
    try {
        auto  file   = readAll(path);
        auto  header = container::decodeHeader(file);
        auto& meta   = header["metadata"];
        if (name) meta["name"] = *name;
        if (author) meta["author"] = *author;
        meta["timeModified"] = nowMilliseconds();
        util::writeAtomic(path, container::replaceHeader(file, std::move(header)), true);
        return {};
    } catch (std::exception const& e) {
        return e.what();
    }
}
} // namespace levischematic::schematic
