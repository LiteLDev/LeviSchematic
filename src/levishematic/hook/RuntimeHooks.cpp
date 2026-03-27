#include "RuntimeHooks.h"

#include "RenderHook.h"
#include "SelectionHook.h"

namespace levishematic::hook {

void registerRuntimeHooks() {
    registerRenderHooks();
    registerSelectionHooks();
}

void unregisterRuntimeHooks() {
    unregisterSelectionHooks();
    unregisterRenderHooks();
}

} // namespace levishematic::hook
