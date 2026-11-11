#pragma once
#include "MaterialRules.h"
#include "levischematic/schematic/placement/ProjectionQuery.h"
#include "levischematic/verifier/VerifierTypes.h"
#include "mc/world/item/ItemStack.h"
#include <map>

namespace levischematic::survival {
struct ItemRequirement {
    ItemKey     key;
    std::string label;
    unsigned    count = 1;
};
struct BlockItems {
    ItemStack                    pick;
    BuildRule                    rule;
    std::vector<ItemRequirement> materials;
    std::string                  unsupported;
};
ItemKey                      itemKey(ItemStackBase const&);
BlockItems                   resolveBlockItems(placement::PlacementInstance const&, placement::ExpectedPosition const&);
verifier::VerificationStatus compareBlocks(Block const* expected, Block const* actual);
bool                         placementOrientationMatches(Block const& expected, Block const& predicted);
} 
