#include "SelectionHook.h"

#include "levischematic/LeviSchematic.h"
#include "levischematic/app/AppKernel.h"
#include "levischematic/selection/SelectionState.h"

#include "ll/api/memory/Hook.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/game/LevelRendererCamera.h"
#include "mc/common/Globals.h"
#include "mc/common/client/renderer/helpers/MeshHelpers.h"
#include "mc/deps/minecraft_renderer/objects/ViewRenderObject.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#include "mc/deps/renderer/ShaderColor.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/VanillaItemNames.h"
#include "mc/world/level/BlockPos.h"

#include <array>

namespace levischematic::hook {
namespace {

bool gSelectionHooksRegistered = false;


auto& getLogger() { return levischematic::LeviSchematic::getInstance().getSelf().getLogger(); }


struct WireframeQuad {
    std::array<Vec3, 4> quad;
    int                 color;
};


struct Wireframe {
    BlockPos                      origin;
    BlockPos                      size;
    std::array<WireframeQuad, 24> quadList;
};


std::array<WireframeQuad, 24> GenerateStructureBlockWireframe(BlockPos const& size) {
    float const E = 0.005f;
    float const C = 0.015f;

    float const x0 = -E;
    float const x1 = size.x + E;
    float const y0 = -E;
    float const y1 = size.y + E;
    float const z0 = -E;
    float const z1 = size.z + E;

    float const xn = size.x - C;
    float const yn = size.y - C;
    float const zn = size.z - C;

    using V3 = Vec3;
    std::array<WireframeQuad, 24> quadList;
    int                           i = 0;

    auto makeY =


        [&](float x, float z, float wx, float wz, int col) {
            quadList[i++] = {{{V3{x, y0, z}, V3{x, y1, z}, V3{wx, yn, z}, V3{wx, C, z}}}, col};
            quadList[i++] = {{{V3{x, y0, z}, V3{x, y1, z}, V3{x, yn, wz}, V3{x, C, wz}}}, col};
        };
    auto makeX =


        [&](float y, float z, float wy, float wz, int col) {
            quadList[i++] = {{{V3{x0, y, z}, V3{x1, y, z}, V3{xn, wy, z}, V3{C, wy, z}}}, col};
            quadList[i++] = {{{V3{x0, y, z}, V3{x1, y, z}, V3{xn, y, wz}, V3{C, y, wz}}}, col};
        };
    auto makeZ =


        [&](float x, float y, float wx, float wy, int col) {
            quadList[i++] = {{{V3{x, y, z0}, V3{x, y, z1}, V3{wx, y, zn}, V3{wx, y, C}}}, col};
            quadList[i++] = {{{V3{x, y, z0}, V3{x, y, z1}, V3{x, wy, zn}, V3{x, wy, C}}}, col};
        };

    uint32_t const colR = 0xFFFF0000;
    uint32_t const colG = 0xFF00FF00;
    uint32_t const colB = 0xFF0000FF;
    uint32_t const colW = 0xFFFFFFFF;

    makeY(-E, -E, C, C, colG);
    makeY(x1, -E, xn, C, colW);
    makeY(x1, z1, xn, zn, colW);
    makeY(-E, z1, C, zn, colW);

    makeX(-E, -E, C, C, colR);
    makeX(y1, -E, yn, C, colW);
    makeX(y1, z1, yn, zn, colW);
    makeX(-E, z1, C, zn, colW);

    makeZ(-E, -E, C, C, colB);
    makeZ(x1, -E, xn, C, colW);
    makeZ(x1, y1, xn, yn, colW);
    makeZ(-E, y1, C, yn, colW);

    return quadList;
}


void tessellateWireBox(Tessellator& tess, AABB const& bb) {
    tess.begin({}, (mce::PrimitiveMode)4, 0, false);
    tess.vertex(bb.max.x, bb.min.y, bb.min.z);
    tess.vertex(bb.min.x, bb.min.y, bb.min.z);
    tess.vertex(bb.min.x, bb.max.y, bb.min.z);
    tess.vertex(bb.max.x, bb.max.y, bb.min.z);
    tess.vertex(bb.min.x, bb.min.y, bb.max.z);
    tess.vertex(bb.max.x, bb.min.y, bb.max.z);
    tess.vertex(bb.min.x, bb.max.y, bb.max.z);
    tess.vertex(bb.max.x, bb.max.y, bb.max.z);
    tess.vertex(bb.min.x, bb.min.y, bb.min.z);
    tess.vertex(bb.min.x, bb.min.y, bb.max.z);
    tess.vertex(bb.max.x, bb.min.y, bb.min.z);
    tess.vertex(bb.max.x, bb.min.y, bb.max.z);
    tess.vertex(bb.min.x, bb.max.y, bb.min.z);
    tess.vertex(bb.min.x, bb.max.y, bb.max.z);
    tess.vertex(bb.max.x, bb.max.y, bb.min.z);
    tess.vertex(bb.max.x, bb.max.y, bb.max.z);
    tess.vertex(bb.min.x, bb.min.y, bb.min.z);
    tess.vertex(bb.min.x, bb.max.y, bb.min.z);
    tess.vertex(bb.max.x, bb.min.y, bb.min.z);
    tess.vertex(bb.max.x, bb.max.y, bb.min.z);
    tess.vertex(bb.min.x, bb.min.y, bb.max.z);
    tess.vertex(bb.min.x, bb.max.y, bb.max.z);
    tess.vertex(bb.max.x, bb.min.y, bb.max.z);
    tess.vertex(bb.max.x, bb.max.y, bb.max.z);
}


void DrawAxisLines(
    ScreenContext&          screenCtx,
    Tessellator&            tess,
    mce::MaterialPtr const& material,
    Wireframe const&        wf,
    glm::vec3 const&        cameraPos
) {
    auto currentMat            = screenCtx.camera.worldMatrixStack->push(false);
    currentMat.stack->_isDirty = true;

    auto* mat                  = currentMat.mat;
    mat->_m                    = glm::translate(mat->_m.get(), -cameraPos);
    currentMat.stack->_isDirty = true;
    mat->_m                    = glm::translate(mat->_m.get(), {wf.origin.x, wf.origin.y, wf.origin.z});

    tess.begin({}, mce::PrimitiveMode::QuadList, 96, false);
    for (auto& quad : wf.quadList) {
        tess.color(mce::Color(quad.color));
        tess.vertex(quad.quad[0].x, quad.quad[0].y, quad.quad[0].z);
        tess.vertex(quad.quad[1].x, quad.quad[1].y, quad.quad[1].z);
        tess.vertex(quad.quad[2].x, quad.quad[2].y, quad.quad[2].z);
        tess.vertex(quad.quad[3].x, quad.quad[3].y, quad.quad[3].z);
    }

    OffscreenCaptureDescription captureDesc{};
    MeshHelpers::renderMeshImmediately(screenCtx, tess, material, captureDesc);

    currentMat.stack->_isDirty = true;
    if (currentMat.stack->sortOrigin->has_value()
        && (currentMat.stack->stack->size() - 1) <= currentMat.stack->sortOrigin->value()) {
        currentMat.stack->sortOrigin->reset();
    }
    currentMat.stack->stack->pop_back();
    currentMat.mat   = nullptr;
    currentMat.stack = nullptr;
}


void DrawPosLine(
    ScreenContext&          screenCtx,
    Tessellator&            tess,
    mce::MaterialPtr const& material,
    BlockPos const&         pos,
    Vec3 const&             cameraPos
) {
    AABB box{
        Vec3{(float)pos.x, (float)pos.y, (float)pos.z},
        Vec3{pos.x + 1.0f, pos.y + 1.0f, pos.z + 1.0f}
    };

    tess.mPostTransformOffset->x = -cameraPos.x;
    tess.mPostTransformOffset->y = -cameraPos.y;
    tess.mPostTransformOffset->z = -cameraPos.z;
    tessellateWireBox(tess, box);
    tess.mPostTransformOffset = Vec3::ZERO();

    screenCtx.currentShaderColor.color = mce::Color::YELLOW();
    screenCtx.currentShaderColor.dirty = true;

    OffscreenCaptureDescription captureDesc{};

    MeshHelpers::renderMeshImmediately(screenCtx, tess, material, captureDesc);
}

} 


LL_TYPE_INSTANCE_HOOK(
    SelectionRenderWireframeHook,
    ll::memory::HookPriority::Normal,
    LevelRendererCamera,
    &LevelRendererCamera::$render,
    void,
    BaseActorRenderContext& renderContext,
    ViewRenderObject const& renderObj,
    IClientInstance&        clientInstance
) {
    origin(renderContext, renderObj, clientInstance);

    auto session = app::tryAccessSession();
    if (!session) {
        return;
    }

    auto overlay = session->selection().overlay();
    if (overlay.selectionMode) {
        if (overlay.corner1) {
            DrawPosLine(
                renderContext.mScreenContext,
                renderContext.mScreenContext.tessellator,
                this->wireframeMaterial,
                *overlay.corner1,
                this->mCameraTargetPos
            );
        }
        if (overlay.corner2) {
            DrawPosLine(
                renderContext.mScreenContext,
                renderContext.mScreenContext.tessellator,
                this->wireframeMaterial,
                *overlay.corner2,
                this->mCameraTargetPos
            );
        }
    }

    if (overlay.origin && overlay.size) {
        Wireframe wireframe{*overlay.origin, *overlay.size, GenerateStructureBlockWireframe(*overlay.size)};
        DrawAxisLines(
            renderContext.mScreenContext,
            renderContext.mScreenContext.tessellator,
            this->wireframeMaterial,
            wireframe,
            renderObj.mViewData->mCameraTargetPos
        );
    }
}


LL_TYPE_INSTANCE_HOOK(
    SelectionClickPos2Hook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$useItemOn,
    InteractionResult,
    ItemStack&        item,
    ::BlockPos const& at,
    uchar             face,
    ::Vec3 const&     hit,
    ::HandSlot        handSlot,
    Block const*      targetBlock,
    bool              isFirstEvent
) {
    auto session = app::accessSession();
    if (!session) {
        return origin(item, at, face, hit, handSlot, targetBlock, isFirstEvent);
    }

    auto& selectionService = session->selection();
    if (isFirstEvent && selectionService.isSelectionModeEnabled()
        && item.isInstance(VanillaItemNames::Stick(), false)) {
        selectionService.setSelectionCorner2(at);
        getLogger().debug("Selection pos2: {}", at.toString());
        return InteractionResult{true, false};
    }
    session.reset();
    return origin(item, at, face, hit, handSlot, targetBlock, isFirstEvent);
}


LL_TYPE_INSTANCE_HOOK(
    SelectionClickPos1Hook,
    HookPriority::Normal,
    GameMode,
    &GameMode::$startDestroyBlock,
    bool,
    BlockPos const& pos,
    uchar           face,
    bool&           hasDestroyedBlock
) {
    auto session = app::accessSession();
    if (!session) {
        return origin(pos, face, hasDestroyedBlock);
    }

    auto& selectionService = session->selection();
    auto& item             = this->mPlayer.mInventory->mInventory->getItem(this->mPlayer.mInventory->mSelected);

    if (selectionService.isSelectionModeEnabled() && item.isInstance(VanillaItemNames::Stick(), false)) {
        selectionService.setSelectionCorner1(pos);
        getLogger().debug("Selection pos1: {}", pos.toString());
        return false;
    }
    session.reset();
    return origin(pos, face, hasDestroyedBlock);
}

using selectionHook =
    ll::memory::HookRegistrar<SelectionRenderWireframeHook, SelectionClickPos2Hook, SelectionClickPos1Hook>;

void registerSelectionHooks() {
    if (gSelectionHooksRegistered) {
        return;
    }
    selectionHook::hook();
    gSelectionHooksRegistered = true;
}

void unregisterSelectionHooks() {
    if (!gSelectionHooksRegistered) {
        return;
    }
    selectionHook::unhook();
    gSelectionHooksRegistered = false;
}

} 
