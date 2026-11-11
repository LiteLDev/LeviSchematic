#include "InventoryAccess.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/inventory/network/ItemStackNetManagerBase.h"
#include "mc/world/inventory/transaction/InventoryAction.h"
#include "mc/world/inventory/transaction/InventoryTransactionManager.h"
#include "mc/world/item/Item.h"

namespace levischematic::survival {
std::map<ItemKey, uint64_t> inventoryCounts(LocalPlayer& player) {
    std::map<ItemKey, uint64_t> result;
    auto&                       inventory = player.getInventory();
    for (int slot = 0; slot < inventory.getContainerSize(); ++slot) {
        auto const& item = inventory.getItem(slot);
        if (item) result[itemKey(item)] += item.mCount;
    }
    return result;
}
bool buildItemMatches(ItemStackBase const& actual, ItemStackBase const& wanted, ItemMatch match) {
    return actual && wanted && itemKey(actual) == itemKey(wanted)
        && (match == ItemMatch::Material || (actual.matchesItem(wanted) && actual.hasSameUserData(wanted)));
}
InventoryResult selectBuildItem(
    LocalPlayer&            player,
    ItemStack const&        wanted,
    std::vector<int> const& slots,
    ItemMatch               match,
    bool                    allowCreate
) {
    if (!wanted) return InventoryResult::Unsupported;
    auto& inventory = player.getInventory();
    auto  manager   = player.mItemStackNetManager.get();
    
    if (manager && !manager->allowInventoryTransactionManager()) return InventoryResult::Busy;
    if (buildItemMatches(player.getSelectedItem(), wanted, match)) return InventoryResult::Ready;

    int source = -1, target = -1;
    for (int slot = 0; slot < inventory.getContainerSize(); ++slot) {
        auto const& item = inventory.getItem(slot);
        if (buildItemMatches(item, wanted, match)) {
            source = slot;
            break;
        }
    }
    if (source >= 0 && source < 9) {
        player.mInventory->selectSlot(source, ContainerID::Inventory);
        return InventoryResult::Ready;
    }
    if (source < 0 && (!player.isCreative() || !allowCreate)) return InventoryResult::Missing;
    for (int slot : slots)
        if (slot >= 0 && slot < 9 && slot < inventory.getContainerSize() && !inventory.getItem(slot)) {
            target = slot;
            break;
        }
    if (target < 0)
        for (int slot : slots)
            if (slot >= 0 && slot < 9 && slot < inventory.getContainerSize()) {
                auto const& item = inventory.getItem(slot);
                if (!item || !item.isDamageableItem()) {
                    target = slot;
                    break;
                }
            }
    if (target < 0) return InventoryResult::NoSlot;
    ItemStack       oldTarget(inventory.getItem(target));
    ItemStack       replacement(source >= 0 ? inventory.getItem(source) : wanted);
    InventorySource container{};
    container.mType        = InventorySourceType::ContainerInventory;
    container.mContainerId = ContainerID::Inventory;
    InventoryAction to(container, static_cast<uint>(target), oldTarget, replacement);
    auto            execute = [&] {
        if (source >= 0) {
            InventoryAction from(container, static_cast<uint>(source), replacement, oldTarget);
            
            inventory.swapSlots(source, target);
            player.mTransactionManager->addAction(to, false);
            player.mTransactionManager->addAction(from, false);
        } else {
            
            
            InventorySource creative{};
            creative.mType        = InventorySourceType::CreativeInventory;
            creative.mContainerId = ContainerID::None;
            InventoryAction discard(creative, 0, ItemStack::EMPTY_ITEM(), oldTarget);
            InventoryAction consume(creative, 1, replacement, ItemStack::EMPTY_ITEM());
            if (oldTarget) player.mTransactionManager->addAction(discard, false);
            player.mTransactionManager->addAction(consume, false);
            
            
            
            inventory.setItem(target, replacement);
        }
    };
    if (manager) {
        auto scope = manager->_tryBeginClientLegacyTransactionRequest();
        execute();
    } else execute();
    player.mInventory->selectSlot(target, ContainerID::Inventory);
    return InventoryResult::Submitted;
}
} 
