#pragma once
#include "mc/world/level/BlockPos.h"
#include <optional>
class Block;
namespace levischematic::placement {
struct PlacementInstance;
}
namespace levischematic::render {
struct FakeWorld;

void createSchematicBlockEntity(
    placement::PlacementInstance const&,
    FakeWorld&,
    BlockPos,
    Block const*,
    std::optional<BlockPos> source
);
} 
