#pragma once
#include "levischematic/schematic/io/MergeExtent.h"
#include "mc/world/level/BlockPos.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

class CompoundTag;

namespace levischematic::schematic {
struct MergeRegion {
    BlockPos           offset;
    CompoundTag const* tag = nullptr;
};

std::unique_ptr<CompoundTag> mergeStructures(std::span<MergeRegion const> regions, BlockPos worldOrigin);
} // namespace levischematic::schematic
