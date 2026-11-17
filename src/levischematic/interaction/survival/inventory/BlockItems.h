#pragma once
#include "levischematic/interaction/survival/inventory/MaterialRules.h"
#include "levischematic/schematic/placement/query/ProjectionQuery.h"
#include "levischematic/verifier/model/VerifierTypes.h"
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
