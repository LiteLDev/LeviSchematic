#pragma once
#include "levischematic/schematic/convert/LitematicMetadata.h"
#include "mc/world/level/BlockPos.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

class CompoundTag;

namespace levischematic::schematic {
inline constexpr char const* LeviSchematicExtension = ".levischematic";

[[nodiscard]] bool isLeviSchematicPath(std::filesystem::path const& path);

struct LeviSchematicRegionRef {
    std::string        id, name;
    BlockPos           offset;
    CompoundTag const* tag = nullptr;
};
struct LeviSchematicWriteInfo {
    std::string        name, author, description;
    int64_t            timeCreated = 0;
    std::array<int, 3> origin{};
};
std::string encodeLeviSchematicFile(std::span<LeviSchematicRegionRef const>, LeviSchematicWriteInfo const&);

struct LeviSchematicRegionInfo {
    std::string        id, name;
    BlockPos           offset;
    std::array<int, 3> size{};
    int64_t            blocks = -1;
};
struct LeviSchematicHeaderInfo {
    convert::LitematicMetadata           metadata;
    std::array<int, 3>                   origin{};
    std::vector<LeviSchematicRegionInfo> regions;
};
LeviSchematicHeaderInfo readLeviSchematicHeader(std::filesystem::path const& path);

struct LeviSchematicRegion {
    std::string                  id, name;
    BlockPos                     offset;
    std::unique_ptr<CompoundTag> tag;
};
struct LeviSchematicDocument {
    std::string                      name;
    std::vector<LeviSchematicRegion> regions;
};
LeviSchematicDocument readLeviSchematic(std::filesystem::path const& path, std::stop_token stop = {});

std::string editLeviSchematicMetadata(
    std::filesystem::path const&      path,
    std::optional<std::string> const& name,
    std::optional<std::string> const& author
);
} // namespace levischematic::schematic
