#pragma once
#include "levischematic/workspace/config/ConfigStore.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/item/ItemStack.h"
namespace levischematic::app {
inline bool isToolItem(ItemStackBase const& item) {
    auto                      name = configStore().snapshot().tool.item;
    thread_local HashedString identifier;
    if (identifier.getString() != name) identifier = HashedString(name);
    return item.isInstance(identifier, false);
}
}
