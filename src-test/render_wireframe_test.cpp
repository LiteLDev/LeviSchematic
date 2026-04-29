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
#include "levischematic/LeviSchematic.h"

#include "ll/api/memory/Hook.h"

#include "mc/client/renderer/game/LevelRendererCamera.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/common/client/renderer/helpers/MeshHelpers.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include "mc/world/level/levelgen/structure/StructureManager.h"

// ─────────────────────────────────────────────────────────────────
// § 1  基础类型
// ─────────────────────────────────────────────────────────────────

struct WireframeQuad {
    std::array<Vec3,4> quad;
    int color;
};

struct Wireframe {
    BlockPos origin;   // 线框左下后角，世界坐标（整数方块坐标）
    BlockPos size;     // 尺寸，单位：方块（各轴 ≥ 1）
    std::array<WireframeQuad,24> quadList;
};

struct ClickBlockPoss {
    BlockPos pos1 = BlockPos::ZERO();
    BlockPos pos2 = BlockPos::ZERO();
};

auto& logger = levischematic::LeviShematic::getInstance().getSelf().getLogger();


// ─────────────────────────────────────────────────────────────────
// § 2  全局状态
// ─────────────────────────────────────────────────────────────────

// 线框渲染所需顶点和颜色
static Wireframe g_frames;
// 鼠标的左键和右键选择坐标
static ClickBlockPoss clickPos;

// ─────────────────────────────────────────────────────────────────
// § 3  计算所有面顶点
// ─────────────────────────────────────────────────────────────────
//
// 每条棱有两个细长矩形以L形式组成，所有有12棱x 2细长矩形 = 24矩形
// 同时设置 3 条从 origin 出发、沿各轴延伸的彩色线段：
//   +X → 橙黄，长度 size.x
//   +Y → 绿色，长度 size.y
//   +Z → 蓝色，长度 size.z
//
// 与游戏结构方块线框角点处的轴向颜色完全一致。

static std::array<WireframeQuad,24> GenerateStructureBlockWireframe(const BlockPos& size) {
    const float E = 0.005f, C = 0.015f;

    // 外边缘 (0面和size面往外扩 E)
    const float x0 = -E, x1 = size.x + E;
    const float y0 = -E, y1 = size.y + E;
    const float z0 = -E, z1 = size.z + E;

    // 内边缘 (从size面向内缩 C)
    const float xn = size.x - C;
    const float yn = size.y - C;
    const float zn = size.z - C;

    using V3 = Vec3;
    std::array<WireframeQuad,24> quadList;
    int i = 0;

    // ---- 1. Y轴棱 (X和Z坐标固定，Y轴方向延伸) ----
    auto makeY = [&](float x, float z, float wx, float wz, int col) {
        // Quad 1: 贴在 X 面上 (Z固定为z，X内缩到wx)
        quadList[i++] = {{{V3{x, y0, z}, V3{x, y1, z}, V3{wx, yn, z}, V3{wx, C, z}}}, col};
        // Quad 2: 贴在 Z 面上 (X固定为x，Z内缩到wz)
        quadList[i++] = {{{V3{x, y0, z}, V3{x, y1, z}, V3{x, yn, wz}, V3{x, C, wz}}}, col};
    };

    // ---- 2. X轴棱 (Y和Z坐标固定，X轴方向延伸) ----
    auto makeX = [&](float y, float z, float wy, float wz, int col) {
        // Quad 1: 贴在 Y 面上 (Z固定为z)
        quadList[i++] = {{{V3{x0, y, z}, V3{x1, y, z}, V3{xn, wy, z}, V3{C, wy, z}}}, col};
        // Quad 2: 贴在 Z 面上 (Y固定为y)
        quadList[i++] = {{{V3{x0, y, z}, V3{x1, y, z}, V3{xn, y, wz}, V3{C, y, wz}}}, col};
    };

    // ---- 3. Z轴棱 (X和Y坐标固定，Z轴方向延伸) ----
    auto makeZ = [&](float x, float y, float wx, float wy, int col) {
        // Quad 1: 贴在 X 面上 (Y固定为y)
        quadList[i++] = {{{V3{x, y, z0}, V3{x, y, z1}, V3{wx, y, zn}, V3{wx, y, C}}}, col};
        // Quad 2: 贴在 Y 面上 (X固定为x)
        quadList[i++] = {{{V3{x, y, z0}, V3{x, y, z1}, V3{x, wy, zn}, V3{x, wy, C}}}, col};
    };

    // 常用颜色定义 (支持类似原点坐标轴的颜色区分)
    const uint32_t colR = 0xFFFF0000; // X轴红色
    const uint32_t colG = 0xFF00FF00; // Y轴绿色
    const uint32_t colB = 0xFF0000FF; // Z轴蓝色
    const uint32_t colW = 0xFFFFFFFF; // 纯白

    // ---- 沿 Y 轴的 4 条棱 ----
    makeY(-E, -E,  C,  C, colG); // 原点 Y 轴 (绿色)
    makeY(x1, -E, xn,  C, colW); 
    makeY(x1, z1, xn, zn, colW); 
    makeY(-E, z1,  C, zn, colW);

    // ---- 沿 X 轴的 4 条棱 ----
    makeX(-E, -E,  C,  C, colR); // 原点 X 轴 (红色)
    makeX(y1, -E, yn,  C, colW); 
    makeX(y1, z1, yn, zn, colW); 
    makeX(-E, z1,  C, zn, colW); 

    // ---- 沿 Z 轴的 4 条棱 ----
    makeZ(-E, -E,  C,  C, colB); // 原点 Z 轴 (蓝色)
    makeZ(x1, -E, xn,  C, colW); 
    makeZ(x1, y1, xn, yn, colW); 
    makeZ(-E, y1,  C, yn, colW);

    return quadList;
}

// ─────────────────────────────────────────────────────────────────
// §4   提交线框渲染
// ─────────────────────────────────────────────────────────────────

static void DrawAxisLines(ScreenContext& screenCtx,
    Tessellator& tess,
    const mce::MaterialPtr& material,
    const Wireframe& wf,
    // const Vec3& cameraPos)
    const glm::vec3& cameraPos)
{
    auto currentMat = screenCtx.camera.worldMatrixStack->push(false);
    currentMat.stack->_isDirty = true;

    glm::vec3 negTarget = -cameraPos;
    auto* mat = currentMat.mat;

    // 执行第一次平移 (将坐标系原点移动到目标位置的相反方向)
    mat->_m = glm::translate(mat->_m.get(), negTarget);

    currentMat.stack->_isDirty = true;

    // 执行第二次平移 (根据玩家位置进行偏移)
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
        std::variant<std::monostate,
            UIActorOffscreenCaptureDescription,
            UIThumbnailMeshOffscreenCaptureDescription,
            UIMeshOffscreenCaptureDescription,
            UIStructureVolumeOffscreenCaptureDescription> od;
        MeshHelpers::renderMeshImmediately(screenCtx, tess, material, od);
    }
    currentMat.stack->_isDirty = true;
    if(currentMat.stack->sortOrigin->has_value() && (currentMat.stack->stack->size()-1) <= currentMat.stack->sortOrigin->value())
        currentMat.stack->sortOrigin->reset();
    currentMat.stack->stack->pop_back();
    currentMat.mat = nullptr;
    currentMat.stack = nullptr;
}

#include "mc/common/Globals.h"
#include "mc/deps/renderer/ShaderColor.h"


// 提交选择点方块的线框渲染
static void DrawPosLine(
    ScreenContext& screenCtx,
    Tessellator& tess,
    const mce::MaterialPtr& material,
    BlockPos& pos,
    const Vec3& cameraPos) {

AABB box{
    Vec3{pos.x, pos.y, pos.z},
    Vec3{pos.x+1,pos.y+1, pos.z+1}
};

// 2) 设置相机偏移
tess.mPostTransformOffset->x = -cameraPos.x;
tess.mPostTransformOffset->y = -cameraPos.y;
tess.mPostTransformOffset->z = -cameraPos.z;

// 3) 生成线框几何
tessellateWireBox(tess, box);
// tessellateThickWireBox(screenCtx, tess, box);//粗线方式，备用

// 4) 复位偏移
tess.mPostTransformOffset = Vec3::ZERO();

// 5) 设置颜色 + 材质并提交
screenCtx.currentShaderColor.color = mce::Color::YELLOW();
screenCtx.currentShaderColor.dirty = true;

std::variant<std::monostate,
            UIActorOffscreenCaptureDescription,
            UIThumbnailMeshOffscreenCaptureDescription,
            UIMeshOffscreenCaptureDescription,
            UIStructureVolumeOffscreenCaptureDescription> captureDesc{};

MeshHelpers::renderMeshImmediately(screenCtx, tess, material, captureDesc);

}

// ─────────────────────────────────────────────────────────────────
// § 5  Hook 函数
// ─────────────────────────────────────────────────────────────────
//
// Hook 目标：LevelRendererCamera::renderStructureWireframes
//
// 选择理由
// ┌─────────────────────────────────────────────────────────┐
// │ ① 每帧必经，保证轴向线与白色线框同帧渲染，无闪烁。          │
// │ ② 此处已持有 ScreenContext / Tessellator / material，    │
// │    无需从游戏内部二次查找这些对象。                        │
// │ ③ 先调原函数（保留游戏线框） → 再追加轴向线，顺序正确。     │
// └─────────────────────────────────────────────────────────┘
//

#include "mc/client/game/ClientInstance.h"
#include "mc/world/actor/player/Player.h"
#include "mc/deps/minecraft_renderer/objects/ViewRenderObject.h"

LL_AUTO_TYPE_INSTANCE_HOOK(
    RenderWireframeHook,
    ll::memory::HookPriority::Normal,
    LevelRendererCamera,
    &LevelRendererCamera::renderStructureWireframes,
    void,
    BaseActorRenderContext& renderContext,
    IClientInstance const&  clientInstance,
    ViewRenderObject const& renderObj
){
    origin(renderContext, clientInstance, renderObj);
    // 当左键坐标不为零时渲染
    if (clickPos.pos1 != BlockPos::ZERO()) {
        DrawPosLine(
            renderContext.mScreenContext,
            renderContext.mScreenContext.tessellator,
            this->wireframeMaterial,
            clickPos.pos1,
            this->mCameraTargetPos
        );
    }
    // 当右键坐标不为零时渲染
    if (clickPos.pos2 != BlockPos::ZERO()) {
        DrawPosLine(
            renderContext.mScreenContext,
            renderContext.mScreenContext.tessellator,
            this->wireframeMaterial,
            clickPos.pos2,
            this->mCameraTargetPos
        );
    }
    // 当线框的原点坐标不为零时渲染线框
    if (g_frames.origin == BlockPos::ZERO()) return;
        DrawAxisLines(
            renderContext.mScreenContext,
            renderContext.mScreenContext.tessellator,
            this->wireframeMaterial,
            g_frames,
            renderObj.mViewData->mCameraTargetPos);
}

// ─────────────────────────────────────────────────────────────────
// § 6  公开 API
// ─────────────────────────────────────────────────────────────────

// RegisterWireframe
// ──────────────────
// 向 g_frames 插入一条线框记录
// 线框从下一帧开始持续渲染，直到调用 UnregisterWireframe 为止。
//
// 参数：
//   origin     线框左下后角的世界整数坐标
//   size       线框在 X/Y/Z 三个方向上的方块数（各轴 ≥ 1）
void RegisterWireframe(BlockPos origin, BlockPos size)
{
    g_frames = {origin, size, GenerateStructureBlockWireframe(size)};
}

// UnregisterWireframe
// ─────────────────────
// 取消渲染选择线框，重置原点坐标即可
// 调用后下一帧起线框消失。
//
void UnregisterWireframe()
{
    g_frames.origin = BlockPos::ZERO();
}

#include "ll/api/service/Bedrock.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/levelgen/structure/StructureSettings.h"
#include "ll/api/service/ServerInfo.h"
#include "ll/api/io/FileUtils.h"
#include "mc/nbt/CompoundTag.h"

// ─────────────────────────────────────────────────────────────────
// § 7  保持选区方块信息
// ─────────────────────────────────────────────────────────────────
static void saveStructureInfo(std::string name,
Dimension& dimension,
StructureSettings& setting) {
    std::filesystem::path structurePath;

    auto structuredata = std::make_unique<StructureTemplate>(name,ll::service::getLevel()->getUnknownBlockTypeRegistry());
    structuredata->fillFromWorld(dimension.getBlockSourceFromMainChunkSource(), g_frames.origin, setting);
    auto nbttag = structuredata->save();
    auto filename = name + ".mcstructure";
    structurePath /= ll::getWorldPath().value();
    structurePath = structurePath.parent_path().parent_path();
    structurePath /= u8"schematics";
    structurePath /= filename;
    ll::utils::file_utils::writeFile(structurePath, nbttag->toBinaryNbt(),true);
}

// 从mcstructure文件中读取内容，得到StructureTemplate
static std::unique_ptr<StructureTemplate> readStructureInfo(std::string name) {
    std::filesystem::path structurePath;

    auto structuredata = std::make_unique<StructureTemplate>(name,ll::service::getLevel()->getUnknownBlockTypeRegistry());
    auto filename = name + ".mcstructure";
    structurePath /= ll::getWorldPath().value();
    structurePath = structurePath.parent_path().parent_path();
    structurePath /= u8"schematics";
    structurePath /= filename;
    auto byte = ll::utils::file_utils::readFile(structurePath,true);
    structuredata->load(*CompoundTag::fromBinaryNbt(*byte));
    return std::move(structuredata);
}



// ─────────────────────────────────────────────────────────────────
// § 7  测试实例
// ─────────────────────────────────────────────────────────────────

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/OverloadData.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPositionFloat.h"

struct ParamTest {
    CommandPositionFloat pos;
    std::string file_name;
    enum class Rmode{
        render,
        clear,
        save
    } rmode;
};

void registerCommand(bool isClient) {
    auto& cmd = ll::command::CommandRegistrar::getInstance(isClient).getOrCreateCommand("rendert", "test tttttt");
    cmd.overload<ParamTest>()
        .required("rmode")
        .optional("pos")
        .execute([&](CommandOrigin const& origin, CommandOutput& output, ParamTest const& param, Command const& cmd) {
            auto pos = origin.getExecutePosition(cmd.mVersion, param.pos);
            if (param.rmode == ParamTest::Rmode::render) {
                RegisterWireframe({pos.x,pos.y,pos.z}, {16, 16, 16});
                output.success("render in {}", pos.toString());
            } else {
                UnregisterWireframe();
                output.success("clear in {}", pos.toString());
            }    
        });
    cmd.overload<ParamTest>()
        .required("rmode")
        .required("file_name")
        .execute([&](CommandOrigin const& origin, CommandOutput& output, ParamTest const& param, Command const& cmd) {
            if (param.rmode == ParamTest::Rmode::save) {
                StructureSettings setting = StructureSettings();
                setting.mStructureOffset = BlockPos::ZERO();
                setting.mStructureSize = g_frames.size;
                setting.mIgnoreEntities = false;
                setting.mIgnoreBlocks = false;
                setting.mAllowNonTickingPlayerAndTickingAreaChunks = false;
                saveStructureInfo(param.file_name, *origin.getDimension(), setting);
                output.success("save!");
                return;
            }   
        });
}

#include "mc/scripting/ServerScriptManager.h"

LL_AUTO_TYPE_INSTANCE_HOOK(
    RegisterBuiltinCommands,
    HookPriority::Normal,
    ServerScriptManager,
    &ServerScriptManager::$onServerThreadStarted,
    EventResult,
    ::ServerInstance& ins
) {
    auto res = origin(ins);
    registerCommand(false);
    return res;
}

// ─────────────────────────────────────────────────────────────────
// § 7  使用示例
// ─────────────────────────────────────────────────────────────────
// 当手持木棍时，获取左键和右键选择方块坐标

#include "mc/world/item/Item.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/item/VanillaItemNames.h"

LL_AUTO_TYPE_INSTANCE_HOOK(
    ClickPos2Hook,
    HookPriority::Normal,
    GameMode,
    &GameMode::_sendUseItemOnEvents,
    InteractionResult,
    ItemStack&      item,
        ::BlockPos const& at,
        uchar             face,
        ::Vec3 const&     hit,
        bool              isFirstEvent
) {
    if (isFirstEvent && item.isInstance(VanillaItemNames::Stick(), false)) {
        clickPos.pos2 = at;
        logger.debug("Test pos2: blockpos:{}, hit:{}",at.toString(), hit.toString());
        if(clickPos.pos1 == clickPos.pos2)
            UnregisterWireframe();
        if(clickPos.pos1 != BlockPos::ZERO() && clickPos.pos2 != BlockPos::ZERO() && clickPos.pos1 != clickPos.pos2) {
            BlockPos minPos;
            BlockPos size;
            // 选择最小坐标作为原点
            minPos.x = std::min(clickPos.pos1.x, clickPos.pos2.x);
            minPos.y = std::min(clickPos.pos1.y, clickPos.pos2.y);
            minPos.z = std::min(clickPos.pos1.z, clickPos.pos2.z);
            // 计算线框大小
            size.x = std::abs(clickPos.pos1.x - clickPos.pos2.x) + 1;
            size.y = std::abs(clickPos.pos1.y - clickPos.pos2.y) + 1;
            size.z = std::abs(clickPos.pos1.z - clickPos.pos2.z) + 1;
            RegisterWireframe(minPos, size);
        }
    }
    return origin(item, at, face, hit, isFirstEvent);
}

#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/actor/player/Inventory.h"

LL_AUTO_TYPE_INSTANCE_HOOK(
    ClickPos1Hook,
    HookPriority::Normal,
    GameMode,
    &GameMode::_startDestroyBlock,
    bool,
    BlockPos const& hitPos,
    Vec3 const& vec3,
    uchar hitFace,
    bool& hasDestroyedBlock
) {
    auto& item = this->mPlayer.mInventory->mInventory->getItem(this->mPlayer.mInventory->mSelected);
    if(item.isInstance(VanillaItemNames::Stick(), false)){
        clickPos.pos1 = hitPos;
        logger.debug("Test Pos1: blockpos:{}, hit:{}",hitPos.toString(), vec3.toString());
        if(clickPos.pos1 == clickPos.pos2)
            UnregisterWireframe();
        if(clickPos.pos1 != BlockPos::ZERO() && clickPos.pos2 != BlockPos::ZERO() && clickPos.pos1 != clickPos.pos2) {
            BlockPos minPos;
            BlockPos size;
            // 选择最小坐标作为原点
            minPos.x = std::min(clickPos.pos1.x, clickPos.pos2.x);
            minPos.y = std::min(clickPos.pos1.y, clickPos.pos2.y);
            minPos.z = std::min(clickPos.pos1.z, clickPos.pos2.z);
            // 计算线框大小
            size.x = std::abs(clickPos.pos1.x - clickPos.pos2.x) + 1;
            size.y = std::abs(clickPos.pos1.y - clickPos.pos2.y) + 1;
            size.z = std::abs(clickPos.pos1.z - clickPos.pos2.z) + 1;
            RegisterWireframe(minPos, size);
        }
        // 取消方块破坏操作
        return false;
    }
    return origin(hitPos, vec3, hitFace, hasDestroyedBlock);
}
*/
