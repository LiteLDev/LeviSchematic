#include "PreviewRenderHook.h"

#include "levischematic/app/AppKernel.h"
#include "levischematic/app/PlacementService.h"
#include "levischematic/render/SchematicChunkRenderer.h"
#include "levischematic/schematic/FakeWorld.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/screens/InGamePlayScreen.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/client/renderer/game/LevelRendererCamera.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/deps/minecraft_renderer/objects/FrameRenderObject.h"
#include "mc/deps/minecraft_renderer/objects/ViewRenderObject.h"
#include "mc/world/level/BlockSource.h"

#include <optional>

namespace levischematic::hook {
namespace {

bool registered = false;

} 







LL_TYPE_INSTANCE_HOOK(
    PreviewScreenRenderHook,
    ll::memory::HookPriority::Normal,
    LevelRendererCamera,
    &LevelRendererCamera::$renderBlockEntities,
    void,
    BaseActorRenderContext& renderContext,
    bool renderAlphaLayer
) {
    if (auto session = app::tryAccessSession(); session && !renderAlphaLayer) {
        session->schematicRenderer().renderFrame(renderContext.mScreenContext, session->placement());
    }
    origin(renderContext, renderAlphaLayer);
}







LL_TYPE_INSTANCE_HOOK(
    ValidationHullFinalRenderHook,
    ll::memory::HookPriority::Normal,
    InGamePlayScreen,
    &InGamePlayScreen::$_renderLevel,
    void,
    ScreenContext&           screenContext,
    FrameRenderObject const& renderObj
) {
    origin(screenContext, renderObj);
    if (renderObj.mViews.get().empty()) {
        return;
    }

    if (auto session = app::tryAccessSession()) {
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (!player) {
            return;
        }
        auto const& viewData = renderObj.mViews.get().front().mViewData.get();
        auto const& camera = viewData.mCameraTargetPos.get();
        Vec3 const cameraPosition{camera.x, camera.y, camera.z};
        session->schematicRenderer().renderValidationHull(
            screenContext,
            cameraPosition,
            session->placement(),
            static_cast<int>(player->getDimensionId()),
            session->selection().overlay()
        );
    }
}









LL_TYPE_INSTANCE_HOOK(
    ValidationHullHitSelectHook,
    ll::memory::HookPriority::Normal,
    LevelRendererPlayer,
    &LevelRendererPlayer::renderHitSelect,
    void,
    BaseActorRenderContext& renderContext,
    BlockSource&            region,
    BlockPos const&         position,
    bool                    fancyGraphics
) {
    if (auto session = app::tryAccessSession()) {
        if (session->schematicRenderer().hasValidationHull(
                session->placement(),
                position,
                static_cast<int>(region.getDimensionId())
            )) {
            return;
        }
    }
    origin(renderContext, region, position, fancyGraphics);
}









LL_TYPE_INSTANCE_HOOK(
    ValidationHullOutlineSelectionHook,
    ll::memory::HookPriority::Normal,
    LevelRendererPlayer,
    &LevelRendererPlayer::_renderOutlineSelection,
    void,
    BaseActorRenderContext& renderContext,
    Block const&           block,
    BlockSource&           region,
    BlockPos const&        position
) {
    if (auto session = app::tryAccessSession()) {
        if (session->schematicRenderer().hasValidationHull(
                session->placement(),
                position,
                static_cast<int>(region.getDimensionId())
            )) {
            return;
        }
    }
    origin(renderContext, block, region, position);
}


using PreviewHook = ll::memory::HookRegistrar<
    PreviewScreenRenderHook,
    ValidationHullFinalRenderHook,
    ValidationHullHitSelectHook,
    ValidationHullOutlineSelectionHook
>;

void registerPreviewRenderHooks() {
    if (registered) return;
    PreviewHook::hook();
    registered = true;
}

void unregisterPreviewRenderHooks() {
    if (!registered) return;
    PreviewHook::unhook();
    registered = false;
}
} 
