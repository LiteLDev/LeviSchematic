#include "RuntimeHooks.h"
#include "PreviewRenderHook.h"
#include "ProjectionHudHook.h"
#include "SignHooks.h"
#include "SurvivalHooks.h"
#include "WorldInteractionHooks.h"

#include "SelectionHook.h"
#include "TickHook.h"
#include "levischematic/render/BlockEntityRenderDiagnostics.h"
#include "levischematic/render/BlockEntityHighlight.h"
#include "levischematic/render/ActorProjection.h"

namespace levischematic::hook {

void registerRuntimeHooks() {
    registerSurvivalHooks();
    registerSignHooks();
    registerWorldInteractionHooks();
    render::diagnostics::registerHooks();
    render::registerBlockEntityHighlight();
    render::registerActorProjectionHooks();
    registerPreviewRenderHooks();
    registerSelectionHooks();
    registerProjectionHudHooks();
}

void unregisterRuntimeHooks() {
    unregisterSurvivalHooks();
    unregisterSignHooks();
    unregisterWorldInteractionHooks();
    unregisterProjectionHudHooks();
    unregisterSelectionHooks();
    unregisterPreviewRenderHooks();
    render::unregisterActorProjectionHooks();
    render::unregisterBlockEntityHighlight();
    render::diagnostics::unregisterHooks();
}

} 
