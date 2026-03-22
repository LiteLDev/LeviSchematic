// ============================================================
// render_wireframe_test.cpp
//
// 原始选区线框渲染 + 鼠标选区 Hook 原型代码已整合到正式模块：
//   - hook/SelectionHook.h/cpp        → 选区线框渲染 Hook + 鼠标左右键选区 Hook
//   - selection/SelectionManager.h/cpp → 选区状态管理（角点/保存）
//   - command/Command.cpp              → /schem pos1/pos2/save/selection 命令
//   - core/DataManager.h               → SelectionManager 集成
//
// 此文件保留作为原始测试参考。
// ============================================================

/*
#include "levishematic/LeviShematic.h"
#include "levishematic/selection/SelectionManager.h"

#include "ll/api/memory/Hook.h"

#include "mc/client/renderer/game/LevelRendererCamera.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/common/client/renderer/helpers/MeshHelpers.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/dimension/Dimension.h"

// ─────────────────────────────────────────────────────────────────
// § 1  基础类型
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

static auto& getLogger() {
    return levishematic::LeviShematic::getInstance().getSelf().getLogger();
}

// ─────────────────────────────────────────────────────────────────
// § 2  全局状态
// ─────────────────────────────────────────────────────────────────

// 线框渲染所需顶点和颜色
static Wireframe g_frames;

// ─────────────────────────────────────────────────────────────────
// § 3  计算所有面顶点
// ─────────────────────────────────────────────────────────────────
//
// 每条棱有两个细长矩形以L形式组成，所有有12棱x 2细长矩形 = 24矩形
// 同时设置 3 条从 origin 出发、沿各轴延伸的彩色线段：
//   +X → 红色，长度 size.x
//   +Y → 绿色，长度 size.y
//   +Z → 蓝色，长度 size.z

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
// § 4  提交线框渲染
// ─────────────────────────────────────────────────────────────────

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

#include "mc/common/Globals.h"
#include "mc/deps/renderer/ShaderColor.h"

// 提交选择点方块的线框渲染
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
// § 5  线框管理 API
// ─────────────────────────────────────────────────────────────────

static void RegisterWireframe(BlockPos origin, BlockPos size) {
    g_frames = {origin, size, GenerateStructureBlockWireframe(size)};
}

static void UnregisterWireframe() { g_frames.origin = BlockPos::ZERO(); }

// 根据 SelectionManager 的状态更新线框
static void UpdateWireframeFromSelection() {
    auto& selMgr = levishematic::selection::SelectionManager::getInstance();
    if (selMgr.hasCompleteSelection()) {
        BlockPos minPos = selMgr.getMinCorner();
        BlockPos size   = selMgr.getSize();
        if (selMgr.getCorner1() != selMgr.getCorner2()) {
            RegisterWireframe(minPos, size);
        } else {
            UnregisterWireframe();
        }
    }
}

// ─────────────────────────────────────────────────────────────────
// § 6  渲染 Hook
// ─────────────────────────────────────────────────────────────────

#include "mc/client/game/ClientInstance.h"
#include "mc/deps/minecraft_renderer/objects/ViewRenderObject.h"
#include "mc/world/actor/player/Player.h"

LL_AUTO_TYPE_INSTANCE_HOOK(
    RenderWireframeHook,
    ll::memory::HookPriority::Normal,
    LevelRendererCamera,
    &LevelRendererCamera::renderStructureWireframes,
    void,
    BaseActorRenderContext&  renderContext,
    IClientInstance const&   clientInstance,
    ViewRenderObject const&  renderObj
) {
    origin(renderContext, clientInstance, renderObj);

    auto& selMgr = levishematic::selection::SelectionManager::getInstance();

    if (selMgr.isSelectionMode()) {
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

    if (g_frames.origin != BlockPos::ZERO()) {
        DrawAxisLines(
            renderContext.mScreenContext,
            renderContext.mScreenContext.tessellator,
            this->wireframeMaterial,
            g_frames,
            renderObj.mViewData->mCameraTargetPos
        );
    }
}

// ─────────────────────────────────────────────────────────────────
// § 7  鼠标选区 Hook
// ─────────────────────────────────────────────────────────────────

#include "mc/world/item/Item.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/item/VanillaItemNames.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/actor/player/Inventory.h"

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
    auto& selMgr = levishematic::selection::SelectionManager::getInstance();

    if (isFirstEvent && selMgr.isSelectionMode() && item.isInstance(VanillaItemNames::Stick(), false)) {
        selMgr.setCorner2(at);
        getLogger().debug("Selection pos2: {}", at.toString());
        UpdateWireframeFromSelection();
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
    auto& selMgr = levishematic::selection::SelectionManager::getInstance();
    auto& item =
        this->mPlayer.mInventory->mInventory->getItem(this->mPlayer.mInventory->mSelected);

    if (selMgr.isSelectionMode() && item.isInstance(VanillaItemNames::Stick(), false)) {
        selMgr.setCorner1(hitPos);
        getLogger().debug("Selection pos1: {}", hitPos.toString());
        UpdateWireframeFromSelection();
        return false;
    }
    return origin(hitPos, vec3, hitFace, hasDestroyedBlock);
}

// ─────────────────────────────────────────────────────────────────
// § 8  选区测试命令
// ─────────────────────────────────────────────────────────────────

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/OverloadData.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPositionFloat.h"

struct WireframeTestParam {
    CommandPositionFloat pos;
    enum class Rmode { render, clear } rmode;
};

void registerWireframeTestCommand(bool isClient) {
    auto& cmd =
        ll::command::CommandRegistrar::getInstance(isClient).getOrCreateCommand("wireframe", "Wireframe test");

    cmd.overload<WireframeTestParam>()
        .required("rmode")
        .optional("pos")
        .execute(
            [&](CommandOrigin const& origin, CommandOutput& output, WireframeTestParam const& param, Command const& cmd
            ) {
                auto pos = origin.getExecutePosition(cmd.mVersion, param.pos);
                if (param.rmode == WireframeTestParam::Rmode::render) {
                    RegisterWireframe({(int)pos.x, (int)pos.y, (int)pos.z}, {16, 16, 16});
                    output.success("Wireframe rendered at {}", pos.toString());
                } else {
                    UnregisterWireframe();
                    output.success("Wireframe cleared");
                }
            }
        );
}
*/
