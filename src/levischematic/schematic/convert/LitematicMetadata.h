#pragma once
#include "levischematic/schematic/convert/JavaNbt.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace levischematic::convert {
struct LitematicMetadata {
    std::string        name, author, description;
    int64_t            timeCreated = 0, timeModified = 0;
    int32_t            regionCount = 0;
    int64_t            totalBlocks = -1, totalVolume = -1;
    std::array<int, 3> enclosingSize{};
    int32_t            version = 0, subVersion = 0, dataVersion = 0;
};

LitematicMetadata readLitematicMetadata(nbt::NamedRoot const& root);
void              writeLitematicMetadata(
    nbt::NamedRoot&                   root,
    std::optional<std::string> const& name,
    std::optional<std::string> const& author,
    int64_t                           nowMilliseconds
);
} // namespace levischematic::convert
