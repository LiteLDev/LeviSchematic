#include "levischematic/app/lifecycle/hooks/RuntimeHooks.h"
#include "levischematic/interaction/sign/runtime/SignHooks.h"
#include "levischematic/interaction/survival/runtime/SurvivalHooks.h"
#include "levischematic/interaction/worldedit/runtime/WorldInteractionHooks.h"
#include "levischematic/presentation/hud/runtime/ProjectionHudHook.h"
#include "levischematic/render/chunk/hooks/PreviewRenderHook.h"

#include "levischematic/app/lifecycle/hooks/TickHook.h"
#include "levischematic/render/actor/ActorProjection.h"
#include "levischematic/render/blockentity/BlockEntityHighlight.h"
#include "levischematic/render/blockentity/BlockEntityRenderDiagnostics.h"
#include "levischematic/selection/runtime/SelectionHook.h"

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
