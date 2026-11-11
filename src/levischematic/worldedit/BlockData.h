#pragma once
#include "levischematic/schematic/placement/ProjectionQuery.h"
#include "mc/world/item/ItemStack.h"
class Dimension;
class BlockSource;
namespace levischematic::worldedit {

ItemStack worldBlockItem(BlockSource&, BlockPos const&);
ItemStack copyItem(ItemStackBase const&);
ItemStack projectionBlockItem(placement::PlacementInstance const&, placement::ExpectedPosition const&, Dimension&);
bool      blockMatches(BlockSource&, BlockPos const&, Block const*, ItemStack const& data);
} 
