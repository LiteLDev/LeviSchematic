// ============================================================
// SelectionHook.cpp
// 选区相关 Hook 实现（Phase 4）
//
// 参考 src-test/render_wireframe_test.cpp 中的最小实现，
// 将选区线框渲染、鼠标左右键选区 Hook 整合到正式模块中。
// ============================================================

#include "SelectionHook.h"

#include "levishematic/LeviShematic.h"
#include "levishematic/selection/SelectionManager.h"

#include "ll/api/memory/Hook.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/game/LevelRendererCamera.h"
#include "mc/common/Globals.h"
#include "mc/common/client/renderer/helpers/MeshHelpers.h"
#include "mc/deps/minecraft_renderer/objects/ViewRenderObject.h"
#include "mc/deps/renderer/ShaderColor.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/VanillaItemNames.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/dimension/Dimension.h"

namespace levishematic::hook {

// ─────────────────────────────────────────────────────────────────
// Logger
// ─────────────────────────────────────────────────────────────────

static auto& getLogger() {
    return levishematic::LeviShematic::getInstance().getSelf().getLogger();
}

// ─────────────────────────────────────────────────────────────────
// § 1  线框基础类型
// ─────────────────────────────────────────────────────────────────

struct WireframeQuad {
    std::array<Vec3, 4> quad;
    int                 color;
};

struct Wireframe {
    BlockPos                      origin; // 线框左下后角，世界坐标
    BlockPos                      size;   // 尺寸，单位：方块（各轴 ≥ 1）
    std::array<WireframeQuad, 24> quadList;
};

// ─────────────────────────────────────────────────────────────────
// § 2  计算选区包围盒线框顶点
// ─────────────────────────────────────────────────────────────────
//
// 每条棱有两个细长矩形以L形式组成，12棱 × 2 = 24矩形
// 原点处 3 条棱带坐标轴颜色（红=X, 绿=Y, 蓝=Z），其余白色

static std::array<WireframeQuad, 24> GenerateStructureBlockWireframe(const BlockPos& size) {
    const float E = 0.005f, C = 0.015f;

    const float x0 = -E, x1 = size.x + E;
    const float y0 = -E, y1 = size.y + E;
    const float z0 = -E, z1 = size.z + E;

    const float xn = size.x - C;
    const float yn = size.y - C;
    const float zn = size.z - C;

    using V3 = Vec3;
    std::array<WireframeQuad, 24> quadList;
    int                           i = 0;

    auto makeY = [&](float x, float z, float wx, float wz, int col) {
        quadList[i++] = {{{V3{x, y0, z}, V3{x, y1, z}, V3{wx, yn, z}, V3{wx, C, z}}}, col};
        quadList[i++] = {{{V3{x, y0, z}, V3{x, y1, z}, V3{x, yn, wz}, V3{x, C, wz}}}, col};
    };

    auto makeX = [&](float y, float z, float wy, float wz, int col) {
        quadList[i++] = {{{V3{x0, y, z}, V3{x1, y, z}, V3{xn, wy, z}, V3{C, wy, z}}}, col};
        quadList[i++] = {{{V3{x0, y, z}, V3{x1, y, z}, V3{xn, y, wz}, V3{C, y, wz}}}, col};
    };

    auto makeZ = [&](float x, float y, float wx, float wy, int col) {
        quadList[i++] = {{{V3{x, y, z0}, V3{x, y, z1}, V3{wx, y, zn}, V3{wx, y, C}}}, col};
        quadList[i++] = {{{V3{x, y, z0}, V3{x, y, z1}, V3{x, wy, zn}, V3{x, wy, C}}}, col};
    };

    const uint32_t colR = 0xFFFF0000; // X轴红色
    const uint32_t colG = 0xFF00FF00; // Y轴绿色
    const uint32_t colB = 0xFF0000FF; // Z轴蓝色
    const uint32_t colW = 0xFFFFFFFF; // 纯白

    // 沿 Y 轴的 4 条棱
    makeY(-E, -E, C, C, colG);
    makeY(x1, -E, xn, C, colW);
    makeY(x1, z1, xn, zn, colW);
    makeY(-E, z1, C, zn, colW);

    // 沿 X 轴的 4 条棱
    makeX(-E, -E, C, C, colR);
    makeX(y1, -E, yn, C, colW);
    makeX(y1, z1, yn, zn, colW);
    makeX(-E, z1, C, zn, colW);

    // 沿 Z 轴的 4 条棱
    makeZ(-E, -E, C, C, colB);
    makeZ(x1, -E, xn, C, colW);
    makeZ(x1, y1, xn, yn, colW);
    makeZ(-E, y1, C, yn, colW);

    return quadList;
}

// ─────────────────────────────────────────────────────────────────
// § 3  渲染提交函数
// ─────────────────────────────────────────────────────────────────

// 提交选区包围盒线框渲染
static void DrawAxisLines(
    ScreenContext&          screenCtx,
    Tessellator&            tess,
    const mce::MaterialPtr& material,
    const Wireframe&        wf,
    const glm::vec3&        cameraPos
) {
    auto currentMat            = screenCtx.camera.worldMatrixStack->push(false);
    currentMat.stack->_isDirty = true;

    glm::vec3 negTarget = -cameraPos;
    auto*     mat       = currentMat.mat;

    mat->_m                    = glm::translate(mat->_m.get(), negTarget);
    currentMat.stack->_isDirty = true;
    mat->_m = glm::translate(mat->_m.get(), {wf.origin.x, wf.origin.y, wf.origin.z});

    tess.begin({}, mce::PrimitiveMode::QuadList, 96, false);

    for (auto& quad : wf.quadList) {
        tess.color(mce::Color(quad.color));
        tess.vertex(quad.quad[0].x, quad.quad[0].y, quad.quad[0].z);
        tess.vertex(quad.quad[1].x, quad.quad[1].y, quad.quad[1].z);
        tess.vertex(quad.quad[2].x, quad.quad[2].y, quad.quad[2].z);
        tess.vertex(quad.quad[3].x, quad.quad[3].y, quad.quad[3].z);
    }
    {
        std::variant<
            std::monostate,
            UIActorOffscreenCaptureDescription,
            UIThumbnailMeshOffscreenCaptureDescription,
            UIMeshOffscreenCaptureDescription,
            UIStructureVolumeOffscreenCaptureDescription>
            od;
        MeshHelpers::renderMeshImmediately(screenCtx, tess, material, od);
    }
    currentMat.stack->_isDirty = true;
    if (currentMat.stack->sortOrigin->has_value()
        && (currentMat.stack->stack->size() - 1) <= currentMat.stack->sortOrigin->value())
        currentMat.stack->sortOrigin->reset();
    currentMat.stack->stack->pop_back();
    currentMat.mat   = nullptr;
    currentMat.stack = nullptr;
}

// 提交单个方块位置的线框渲染（黄色）
static void DrawPosLine(
    ScreenContext&          screenCtx,
    Tessellator&            tess,
    const mce::MaterialPtr& material,
    const BlockPos&         pos,
    const Vec3&             cameraPos
) {
    AABB box{Vec3{(float)pos.x, (float)pos.y, (float)pos.z}, Vec3{pos.x + 1.0f, pos.y + 1.0f, pos.z + 1.0f}};

    tess.mPostTransformOffset->x = -cameraPos.x;
    tess.mPostTransformOffset->y = -cameraPos.y;
    tess.mPostTransformOffset->z = -cameraPos.z;

    tessellateWireBox(tess, box);

    tess.mPostTransformOffset = Vec3::ZERO();

    screenCtx.currentShaderColor.color = mce::Color::YELLOW();
    screenCtx.currentShaderColor.dirty = true;

    std::variant<
        std::monostate,
        UIActorOffscreenCaptureDescription,
        UIThumbnailMeshOffscreenCaptureDescription,
        UIMeshOffscreenCaptureDescription,
        UIStructureVolumeOffscreenCaptureDescription>
        captureDesc{};

    MeshHelpers::renderMeshImmediately(screenCtx, tess, material, captureDesc);
}

// ─────────────────────────────────────────────────────────────────
// § 5  渲染 Hook
// ─────────────────────────────────────────────────────────────────
//
// Hook 目标：LevelRendererCamera::renderStructureWireframes
//
// 选择理由：
//   ① 每帧必经，保证线框同帧渲染，无闪烁
//   ② 已持有 ScreenContext / Tessellator / material
//   ③ 先调原函数（保留游戏线框）→ 再追加自定义线框

LL_AUTO_TYPE_INSTANCE_HOOK(
    SelectionRenderWireframeHook,
    ll::memory::HookPriority::Normal,
    LevelRendererCamera,
    &LevelRendererCamera::renderStructureWireframes,
    void,
    BaseActorRenderContext& renderContext,
    IClientInstance const&  clientInstance,
    ViewRenderObject const& renderObj
) {
    origin(renderContext, clientInstance, renderObj);

    auto& selMgr = selection::SelectionManager::getInstance();

    // 当选区模式开启时，渲染角点方块线框
    if (selMgr.isSelectionMode()) {
        // 渲染角点1
        if (selMgr.hasCorner1()) {
            BlockPos c1 = selMgr.getCorner1();
            DrawPosLine(
                renderContext.mScreenContext,
                renderContext.mScreenContext.tessellator,
                this->wireframeMaterial,
                c1,
                this->mCameraTargetPos
            );
        }
        // 渲染角点2
        if (selMgr.hasCorner2()) {
            BlockPos c2 = selMgr.getCorner2();
            DrawPosLine(
                renderContext.mScreenContext,
                renderContext.mScreenContext.tessellator,
                this->wireframeMaterial,
                c2,
                this->mCameraTargetPos
            );
        }
    }

    // 渲染选区包围盒线框（状态由 SelectionManager 统一管理）
    if (auto wireframeState = selMgr.getSelectionWireframeState(); wireframeState.has_value()) {
        Wireframe wf{
            wireframeState->origin,
            wireframeState->size,
            GenerateStructureBlockWireframe(wireframeState->size)
        };
        DrawAxisLines(
            renderContext.mScreenContext,
            renderContext.mScreenContext.tessellator,
            this->wireframeMaterial,
            wf,
            renderObj.mViewData->mCameraTargetPos
        );
    }
}

// ─────────────────────────────────────────────────────────────────
// § 6  鼠标选区 Hook
// ─────────────────────────────────────────────────────────────────
//
// 手持木棍时：
//   左键 → 设置角点1（取消方块破坏）
//   右键 → 设置角点2
// 两个角点都设置后自动生成选区线框

// 右键 Hook → 设置角点2
LL_AUTO_TYPE_INSTANCE_HOOK(
    SelectionClickPos2Hook,
    HookPriority::Normal,
    GameMode,
    &GameMode::_sendUseItemOnEvents,
    InteractionResult,
    ItemStack&        item,
    ::BlockPos const& at,
    uchar             face,
    ::Vec3 const&     hit,
    bool              isFirstEvent
) {
    auto& selMgr = selection::SelectionManager::getInstance();

    if (isFirstEvent && selMgr.isSelectionMode() && item.isInstance(VanillaItemNames::Stick(), false)) {
        selMgr.setCorner2(at);
        getLogger().debug("Selection pos2: {}", at.toString());
        return InteractionResult{InteractionResult::Result::Success};
    }
    return origin(item, at, face, hit, isFirstEvent);
}

// 左键 Hook → 设置角点1
LL_AUTO_TYPE_INSTANCE_HOOK(
    SelectionClickPos1Hook,
    HookPriority::Normal,
    GameMode,
    &GameMode::_startDestroyBlock,
    bool,
    BlockPos const& hitPos,
    Vec3 const&     vec3,
    uchar           hitFace,
    bool&           hasDestroyedBlock
) {
    auto& selMgr = selection::SelectionManager::getInstance();
    auto& item =
        this->mPlayer.mInventory->mInventory->getItem(this->mPlayer.mInventory->mSelected);

    if (selMgr.isSelectionMode() && item.isInstance(VanillaItemNames::Stick(), false)) {
        selMgr.setCorner1(hitPos);
        getLogger().debug("Selection pos1: {}", hitPos.toString());
        // 取消方块破坏操作
        return false;
    }
    return origin(hitPos, vec3, hitFace, hasDestroyedBlock);
}

} // namespace levishematic::hook
