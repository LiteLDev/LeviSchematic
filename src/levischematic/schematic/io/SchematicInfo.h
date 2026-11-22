#pragma once
#include "levischematic/schematic/convert/LitematicMetadata.h"
#include <filesystem>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <string>

class CompoundTag;

namespace levischematic::schematic {
struct SchematicInfo {
    bool                                      ok = false;
    std::string                               error;
    std::string                               format;
    std::optional<convert::LitematicMetadata> litematic;
    std::optional<convert::LitematicMetadata> metadata;
    std::string                               name;
    std::array<int, 3>                        size{};
    int64_t                                   blocks = -1, volume = -1;
    int                                       regions       = 0;
    int                                       formatVersion = 0;
};

struct StructureStats {
    std::array<int, 3> size{};
    int64_t            blocks        = -1;
    int                formatVersion = 0;
};
StructureStats structureStats(CompoundTag const& tag);

SchematicInfo readSchematicInfo(std::filesystem::path const& path);

class SchematicInfoCache {
public:
    std::optional<SchematicInfo> get(std::filesystem::path const& path);
    void                         clear();

private:
    struct Entry {
        std::filesystem::file_time_type   stamp;
        std::shared_future<SchematicInfo> value;
    };
    std::mutex                             mMutex;
    std::map<std::filesystem::path, Entry> mEntries;
};

std::string editLitematicMetadata(
    std::filesystem::path const&      path,
    std::optional<std::string> const& name,
    std::optional<std::string> const& author
);
} // namespace levischematic::schematic
