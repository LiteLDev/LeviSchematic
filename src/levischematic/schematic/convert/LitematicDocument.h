#pragma once
#include "levischematic/schematic/convert/JavaNbt.h"
#include <array>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace levischematic::convert {
inline constexpr int32_t LitematicVersion    = 7;
inline constexpr int32_t LitematicSubVersion = 1;

struct LitematicRegion {
    std::string              name;
    std::array<int, 3>       position{};
    std::array<int, 3>       size{};
    std::vector<std::string> palette;
    std::vector<uint32_t>    blocks;
    size_t                   tileEntities = 0;
    size_t                   entities     = 0;
    size_t                   pendingTicks = 0;
};

struct LitematicDocument {
    int32_t                      version     = LitematicVersion;
    int32_t                      subVersion  = LitematicSubVersion;
    int32_t                      dataVersion = 0;
    std::string                  name;
    std::string                  author;
    std::string                  description;
    int64_t                      timeCreated  = 0;
    int64_t                      timeModified = 0;
    std::vector<LitematicRegion> regions;
};

std::string canonicalJavaState(std::string_view name, std::vector<std::pair<std::string, std::string>> properties);
std::pair<std::string, std::vector<std::pair<std::string, std::string>>> splitJavaState(std::string_view state);

LitematicDocument decodeLitematic(nbt::NamedRoot const& root, std::stop_token stop = {});
nbt::NamedRoot    encodeLitematic(LitematicDocument const& document);
} // namespace levischematic::convert
