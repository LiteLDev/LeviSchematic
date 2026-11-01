#include "RuntimeHooks.h"
#include "PreviewRenderHook.h"

#include "SelectionHook.h"
#include "TickHook.h"
#include "levischematic/render/BlockEntityRenderDiagnostics.h"

namespace levischematic::hook {

void registerRuntimeHooks() {
    render::diagnostics::registerHooks();
    registerPreviewRenderHooks();
    registerSelectionHooks();
}

void unregisterRuntimeHooks() {
    unregisterSelectionHooks();
    unregisterPreviewRenderHooks();
    render::diagnostics::unregisterHooks();
}

} 
