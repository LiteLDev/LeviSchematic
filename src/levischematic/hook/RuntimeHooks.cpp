#include "RuntimeHooks.h"

#include "RenderHook.h"
#include "SelectionHook.h"
#include "TickHook.h"

namespace levischematic::hook {

void registerRuntimeHooks() {
    registerTickHooks();
    registerRenderHooks();
    registerSelectionHooks();
}

void unregisterRuntimeHooks() {
    unregisterSelectionHooks();
    unregisterRenderHooks();
    unregisterTickHooks();
}

} // namespace levischematic::hook
