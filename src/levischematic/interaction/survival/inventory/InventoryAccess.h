#pragma once
#include "levischematic/interaction/survival/inventory/BlockItems.h"
#include <map>
class LocalPlayer;
namespace levischematic::survival {
enum class InventoryResult { Ready, Submitted, Busy, Missing, NoSlot, Unsupported };
std::map<ItemKey, uint64_t> inventoryCounts(LocalPlayer&);
enum class ItemMatch { Material, Complete };
bool            buildItemMatches(ItemStackBase const&, ItemStackBase const&, ItemMatch);
InventoryResult selectBuildItem(
    LocalPlayer&,
    ItemStack const&,
    std::vector<int> const& hotbarSlots,
    ItemMatch               match       = ItemMatch::Material,
    bool                    allowCreate = true
);
} // namespace levischematic::survival
