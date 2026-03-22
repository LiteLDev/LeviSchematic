// ============================================================
// test.cpp
//
// 原始渲染 Hook 原型代码已整合到正式模块：
//   - render/ProjectionRenderer.h/cpp  → ProjectionState, ProjectionSnapshot, ProjEntry
//   - hook/RenderHook.h/cpp            → _sortBlocks Hook, tessellateInWorld Hook
//   - hook/TickHook.h/cpp              → 命令注册 Hook
//   - command/Command.h/cpp            → /rendert, /schem 命令
//   - core/DataManager.h/cpp           → 全局状态中枢
//   - util/PositionUtils.h             → 坐标编码工具
//
// 此文件保留
// ============================================================

/*
#include "mc/client/game/MinecraftGame.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/chunks/RenderChunkBuilder.h"
#include "mc/client/renderer/game/LevelBuilder.h"
#include "mc/client/renderer/game/LevelRenderer.h"

#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/VanillaBlockTypeIds.h"
#include "mc/world/level/block/registry/BlockTypeRegistry.h"
#include "mc/world/level/dimension/Dimension.h"

#include "MyMod.h"

class RenderChunkCoordinator : public LevelListener {
    char unk[0x168];
};

struct BlockQueueEntry {
    BlockPos     pos;       // offset 0x00, 12 bytes
    const Block* blockInfo; // offset 0x10, 8 bytes
};

struct RenderChunkGeometry {
    char     unk[0x34];
    BlockPos mPosition;
    BlockPos mCenter;
    char     unk2[0x1fc];
};

namespace modTest {
auto& logger = my_mod::MyMod::getInstance().getSelf().getLogger();
// ================================================================
// 投影条目（调用方填充）
// ================================================================
struct ProjEntry {
    BlockPos     pos;   // 世界坐标
    const Block* block; // BlockTypeRegistry 管理，生命周期与游戏一致
    mce::Color   color; // RGBA 投影颜色（如 0.7,0.7,1.0,0.45 = 蓝色半透明）
};
static constexpr int RENDERLAYER_BLEND = 3;


//
//
// ================================================================
// Hook 点：RenderChunkBuilder::_updateFacesMetadata
//   mangled: ?_updateFacesMetadata@RenderChunkBuilder@@IEAAXXZ
//   RVA:     0x14201a070 - imageBase(0x140000000) = 0x201a070
//
// 选择依据（来自 RenderChunkBuilder::build 反编译流程）：
//
//   RenderChunkBuilder::build()
//     │
//     ├─ _sortBlocks()          ← 遍历方块，按渲染层分类入队
//     ├─ _tessellateQueues()    ← 调用 BlockTessellator 生成顶点
//     │                            写入 mTessellator->mMeshData   ← 追加投影的最佳时机
//     ├─ _buildRanges()         ← 计算 mRenderLayerRanges（各层在 buffer 的起止偏移）
//     ├─ _updateFacesMetadata() ← 更新透明面排序元数据            ← Hook 此处（函数入口）
//     └─ Tessellator::end()     ← MeshData → mBuiltMesh（CPU→GPU）
//
// 在 _updateFacesMetadata 入口 Hook：
//   · 调用 origin() 后，_tessellateQueues/_buildRanges 均已完成
//   · mTessellator->mMeshData 里有所有原生顶点
//   · 我们追加的投影顶点会被后续 Tessellator::end() 一并打包
//   · 不需要额外的 GPU 资源管理或 Draw Call
// ================================================================

#include "ll/api/memory/Hook.h"

// 游戏类型（来自你提供的头文件）
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/deps/core/math/Color.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/VanillaBlockTypeIds.h"
#include "mc/world/level/block/registry/BlockTypeRegistry.h"


#include <atomic>
#include <mutex>
#include <windows.h>

// ================================================================
// Key 编码工具
//
// 编码方案（uint64_t）：
//   bits[63:42] = (uint32_t)x           (22 bits，负数补码保证唯一)
//   bits[41:21] = (uint32_t)z & 0x1FFFFF (21 bits)
//   bits[20: 0] = (uint32_t)y & 0x1FFFFF (21 bits)
//
// 世界 y 范围 -64~320（9 bits 有余），x/z ±30M（25 bits），
// 用 21 bits mask 会在极端坐标下有碰撞，但投影用途不涉及边界世界，完全足够。
// ================================================================
inline uint64_t encodePosKey(int x, int y, int z) noexcept {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 42)
         | (static_cast<uint64_t>(static_cast<uint32_t>(z) & 0x1FFFFFu) << 21)
         | static_cast<uint64_t>(static_cast<uint32_t>(y) & 0x1FFFFFu);
}

inline uint64_t encodePosKey(const BlockPos& p) noexcept { return encodePosKey(p.x, p.y, p.z); }

// RenderChunkGeometry::mPosition 是 SubChunk 原点（已是16的整数倍，如 x=-64）
// >>4 得 SubChunk 索引（如 -4），用于 HashMap key
inline uint64_t encodeSubChunkKey(const BlockPos& origin) noexcept {
    int sx = origin.x >> 4;
    int sy = origin.y >> 4;
    int sz = origin.z >> 4;
    return (static_cast<uint64_t>(static_cast<uint32_t>(sx)) << 42)
         | (static_cast<uint64_t>(static_cast<uint32_t>(sz) & 0x1FFFFFu) << 21)
         | static_cast<uint64_t>(static_cast<uint32_t>(sy) & 0x1FFFFFu);
}

// 将任意世界坐标转为其所在 SubChunk 的 key（用于 setEntries 分组）
inline uint64_t subChunkKeyFromWorldPos(int wx, int wy, int wz) noexcept {
    // 向下取整除以 16，正确处理负数
    auto     floorDiv16 = [](int v) -> int { return v / 16 - (v % 16 != 0 && v < 0 ? 1 : 0); };
    BlockPos origin{floorDiv16(wx) * 16, floorDiv16(wy) * 16, floorDiv16(wz) * 16};
    return encodeSubChunkKey(origin);
}

// ================================================================
// ProjectionSnapshot：不可变快照（copy-on-write）
// ================================================================
struct ProjectionSnapshot {
    // 按 SubChunk 分组：key = encodeSubChunkKey，value = 该 SubChunk 内的投影块
    std::unordered_map<uint64_t, std::vector<ProjEntry>> bySubChunk;

    // 全体投影块 pos→color，供 tessellateInWorld Hook O(1) 查找
    std::unordered_map<uint64_t, mce::Color> posColorMap;

    bool empty() const { return posColorMap.empty(); }
};

// ================================================================
// 全局投影状态
// ================================================================
struct ProjectionState {
    // 活跃快照（atomic shared_ptr，工作线程无锁读取）
    std::atomic<std::shared_ptr<const ProjectionSnapshot>> snapshot{std::make_shared<const ProjectionSnapshot>()};

    // 原始条目（主线程修改，mutex 保护）
    std::vector<ProjEntry> entries;
    std::mutex             entriesMutex;

    // 重建快照（entriesMutex 已锁定时调用）
    void rebuildSnapshot_locked() {
        auto snap = std::make_shared<ProjectionSnapshot>();
        for (const auto& e : entries) {
            uint64_t scKey = subChunkKeyFromWorldPos(e.pos.x, e.pos.y, e.pos.z);
            snap->bySubChunk[scKey].push_back(e);
            snap->posColorMap[encodePosKey(e.pos)] = e.color;
        }
        snapshot.store(std::move(snap), std::memory_order_release);
    }

    // 完整替换所有投影块（主线程调用）
    void setEntries(std::vector<ProjEntry> newEntries) {
        std::lock_guard<std::mutex> lk(entriesMutex);
        entries = std::move(newEntries);
        rebuildSnapshot_locked();
    }

    // 无锁读取当前快照（工作线程调用）
    std::shared_ptr<const ProjectionSnapshot> getSnapshot() const { return snapshot.load(std::memory_order_acquire); }
};

static ProjectionState gProjection;

// ================================================================
// 线程局部变量
// ================================================================

// 当前 build 持有的快照（_sortBlocks Hook 里 load，保证整个 build 期间有效）
thread_local std::shared_ptr<const ProjectionSnapshot> tl_currentSnapshot;

// 当前 SubChunk 是否有投影块（false = 快速跳过 tessellateInWorld Hook）
thread_local bool tl_hasProjection = false;

// ================================================================
// 公开 API
// ================================================================

// 替换全部投影块（主线程调用）
// 调用后需手动触发受影响 SubChunk 的重建（ProjectionMod_triggerRebuild）
void ProjectionMod_setEntries(std::vector<ProjEntry> entries) { gProjection.setEntries(std::move(entries)); }

// 清空所有投影
void ProjectionMod_clear() { gProjection.setEntries({}); }

// 单个方块便利函数
void ProjectionMod_setSingle(BlockPos pos, const Block* block, mce::Color color) {
    gProjection.setEntries({
        ProjEntry{pos, block, color}
    });
}

// ================================================================
// SubChunk 重建触发
//
// RenderChunkCoordinator::_setDirty（IDA 0x14201fd00）：
//   void _setDirty(const BlockPos& min, const BlockPos& max,
//                  bool immediate, bool changesVisibility, bool canInterlock)
//
//   min/max 是世界坐标，内部 >>4 转 SubChunk 索引。
//   immediate=false → 下一帧重建（推荐，避免卡顿）
//   immediate=true  → 本帧立即重建
//
// renderChunkCoordinator 通过 LevelRenderer::getRenderChunkCoordinator() 获取，
// 建议在渲染帧回调里缓存指针。
// ================================================================
DWORD _setDirty_rva = 0x201fd00;

void ProjectionMod_triggerRebuild(std::shared_ptr<RenderChunkCoordinator> renderChunkCoordinator) {
    if (!renderChunkCoordinator) return;
    auto snap = gProjection.getSnapshot();
    if (!snap || snap->empty()) return;

    HMODULE hModule   = GetModuleHandle(nullptr);
    void*   _setDirty = reinterpret_cast<BYTE*>(hModule) + _setDirty_rva;

    for (const auto& [scKey, vec] : snap->bySubChunk) {
        if (vec.empty()) continue;
        const BlockPos& p = vec[0].pos; // 任意属于该 SubChunk 的坐标即可
        ll::memory::addressCall<void, RenderChunkCoordinator*, BlockPos const&, BlockPos const&, bool, bool, bool>(
            _setDirty,
            renderChunkCoordinator.get(),
            p,
            p,
            false,
            false,
            false
        );
    }
}


// ================================================================
// Hook 1：RenderChunkBuilder::_sortBlocks
//
// 真实签名（IDA 0x14201a600，4个参数，注意无 buildDetails）：
//   char _sortBlocks(BlockSource& region,
//                    RenderChunkGeometry& renderChunkGeometry,
//                    bool airAndSimpleBlocks,
//                    AirAndSimpleBlockBits& airBits)
//
// 职责：
//   1. 原子 load 快照到 tl_currentSnapshot（无锁）
//   2. 用 renderChunkGeometry.mPosition 查 bySubChunk HashMap（O(1)）
//   3. push_back 投影块进 mQueues[BLEND]
//   4. 设置 tl_hasProjection 标志
// ================================================================

LL_AUTO_TYPE_INSTANCE_HOOK(
    ProjectionRenderHook,
    ll::memory::HookPriority::Normal,
    RenderChunkBuilder,
    0x7FF6770BA600,
    bool,
    BlockSource&         region,
    RenderChunkGeometry& renderChunkGeometry,
    bool                 airAndSimpleBlocks,
    void*                airAndSimpleBlockBits,
    void*                buildDetails
) {
    // Step 1: 执行原始 _sortBlocks
    // logger.info("Start!");
    bool result = origin(region, renderChunkGeometry, airAndSimpleBlocks, airAndSimpleBlockBits, buildDetails);

    // load 快照（atomic，无锁，工作线程安全）
    tl_currentSnapshot = gProjection.getSnapshot();
    tl_hasProjection   = false;

    if (!tl_currentSnapshot || tl_currentSnapshot->empty()) {
        return result;
    }

    // renderChunkGeometry.mPosition 是 SubChunk 原点（偏移 0x34，LeviLamina 字段）
    uint64_t scKey = encodeSubChunkKey(renderChunkGeometry.mPosition);

    auto it = tl_currentSnapshot->bySubChunk.find(scKey);
    if (it == tl_currentSnapshot->bySubChunk.end() || it->second.empty()) {
        return result;
    }

    // 有投影块：设置标志，push 进 BLEND 队列
    tl_hasProjection = true;

    const Block* stoneBlock = &BlockTypeRegistry::get().getDefaultBlockState(VanillaBlockTypeIds::RedstoneBlock());
    if (!stoneBlock) {
        return result;
    }

    // ------------------------------------------------------------------
    // Step 4: 向 mQueues[RENDERLAYER_BLEND] push_back 投影块
    //
    // mQueues 是 std::vector<BlockQueueEntry>[17]，偏移 0x150
    // std::vector<BlockQueueEntry> 布局：
    //   _Myfirst (T*)   = vec + 0x00
    //   _Mylast  (T*)   = vec + 0x08
    //   _Myend   (T*)   = vec + 0x10
    //
    // 使用 std::vector::push_back 的等效操作：
    // 若 _Mylast != _Myend，直接原地构造；否则需要 realloc
    // 这里我们直接调用标准的 reinterpret_cast 后的 vector push_back
    // ------------------------------------------------------------------

    for (const auto& e : it->second) {
        BlockQueueEntry entry;
        entry.pos       = e.pos;
        entry.blockInfo = e.block;
        this->mQueues[RENDERLAYER_BLEND].push_back(entry);
    }

    // logger.info("Finsh!");

    return result;
}

// ================================================================
// Hook 2：BlockTessellator::tessellateInWorld（精确颜色控制）
//
// 签名（IDA 0x141f250e0）：
//   bool tessellateInWorld(Tessellator& tessellator,
//                          const Block& block,
//                          const BlockPos& pos,
//                          bool useCalcWithCache)
//
// 性能分层：
//   无投影 SubChunk → 读 1 次 bool，直接 origin（几乎零开销）
//   有投影 SubChunk → 1 次 unordered_map 查找（O(1)）
//     命中（投影块）→ 2 次内存写 + origin + 1 次内存写
//     未命中（真实方块）→ 直接 origin
//
// 草方块变色根治：
//   每次只对命中 pos 的那次调用设置 mColorOverride，origin 返回后立刻清除。
//   其他方块的 tessellateBlockInWorld 读到 _Has_value=false，
//   走正常生物群系色采样路径，草方块绿色完全不受影响。
// ================================================================
LL_AUTO_TYPE_INSTANCE_HOOK(
    ProjectionTessellateQueuesHook,
    ll::memory::HookPriority::Normal,
    BlockTessellator,
    &BlockTessellator::tessellateInWorld,
    bool,
    Tessellator&    tessellator,
    Block const&    block,
    BlockPos const& pos,
    bool            useCalcWithCache
) {
    // 快速路径：当前 SubChunk 无投影块（绝大多数 SubChunk）
    if (!tl_hasProjection) {
        return origin(tessellator, block, pos, useCalcWithCache);
    }

    // O(1) 查 pos 是否是投影块
    auto it = tl_currentSnapshot->posColorMap.find(encodePosKey(pos));
    if (it == tl_currentSnapshot->posColorMap.end()) {
        // 真实方块，完全不碰 mColorOverride
        return origin(tessellator, block, pos, useCalcWithCache);
    }
    // 投影块：精确夹住 mColorOverride
    this->mColorOverride = it->second;

    bool result = origin(tessellator, block, pos, useCalcWithCache);

    // 立刻清除，不影响下一个方块
    this->mColorOverride->reset();
    return result;
}

struct ParamTest {
    CommandBlockName     blockName;
    CommandPositionFloat pos;
    int                  title_date;
    enum class mode { add, single } mode_;
};

static std::vector<ProjEntry> entries;

void registerCommand(bool isClient) {
    auto& cmd = ll::command::CommandRegistrar::getInstance(isClient).getOrCreateCommand("rendert", "test tttttt");
    cmd.overload<ParamTest>()
        .required("pos")
        .required("blockName")
        .required("title_date")
        .required("mode_")
        .execute([&](CommandOrigin const& origin, CommandOutput& output, ParamTest const& param, Command const& cmd) {
            if (param.mode_ == ParamTest::mode::single) {
                ProjectionMod_setSingle(
                    origin.getExecutePosition(cmd.mVersion, param.pos),
                    param.blockName.resolveBlock(param.title_date).mBlock,
                    mce::Color(0.75f, 0.85f, 1.0f, 0.85f)
                );
            } else {
                entries.push_back(
                    {BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos)),
                     param.blockName.resolveBlock(param.title_date).mBlock,
                     mce::Color(0.75f, 0.85f, 1.0f, 0.85f)}
                );
                auto temp = entries;
                ProjectionMod_setEntries(temp);
            }
            std::shared_ptr<RenderChunkCoordinator> rendcoodr =
                ll::service::getClientInstance()->getLevelRenderer()->mRenderChunkCoordinators->at(
                    origin.getDimension()->getDimensionId()
                );
            ProjectionMod_triggerRebuild(rendcoodr);
        });
    cmd.overload<ParamTest>().execute([&](CommandOrigin const& origin, CommandOutput& output) {
        ProjectionMod_clear();
        ProjectionMod_triggerRebuild(ll::service::getClientInstance()->getLevelRenderer()->mRenderChunkCoordinators->at(
            origin.getDimension()->getDimensionId()
        ));
        output.success("cleared");
    });
}

LL_AUTO_TYPE_INSTANCE_HOOK(
    RegisterBuiltinCommands,
    HookPriority::Highest,
    ServerScriptManager,
    &ServerScriptManager::$onServerThreadStarted,
    EventResult,
    ::ServerInstance& ins
) {
    auto res = origin(ins);
    registerCommand(false);
    return res;
}
} // namespace modTest
*/