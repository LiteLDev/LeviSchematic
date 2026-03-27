#pragma once
// ============================================================
// ProjectionRenderer.h
// 投影渲染管理 — 全局投影状态 + 快照管理
//
// 从 test.cpp 提取并整合：
//   - ProjEntry：投影条目
//   - ProjectionSnapshot：不可变快照（按 SubChunk 分组）
//   - ProjectionState：全局投影状态（atomic shared_ptr，lock-free 读取）
//   - triggerRebuild：触发 SubChunk 重建
// ============================================================

#include "levishematic/util/PositionUtils.h"

#include "mc/deps/core/math/Color.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// 前向声明
class RenderChunkCoordinator;

namespace levishematic::render {

// ================================================================
// 投影条目（调用方填充）
// ================================================================
struct ProjEntry {
    BlockPos     pos;   // 世界坐标
    const Block* block; // BlockTypeRegistry 管理，生命周期与游戏一致
    mce::Color   color; // RGBA 投影颜色（如 0.7,0.7,1.0,0.45 = 蓝色半透明）
};

// 渲染层常量
static constexpr int RENDERLAYER_BLEND = 3;

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
// ProjectionState：全局投影状态
// ================================================================
class ProjectionState {
public:
    // 单写多读模型：
    //   - 仅主线程/业务线程写入 entries 并替换快照
    //   - 渲染工作线程只通过 getSnapshot() 读取不可变快照

    // 完整替换所有投影块（主线程调用）
    void setEntries(std::vector<ProjEntry> newEntries);

    // 完整替换所有投影块，并返回替换前的快照。
    std::shared_ptr<const ProjectionSnapshot> replaceEntries(std::vector<ProjEntry> newEntries);

    // 原子业务接口：替换 entries 并立刻触发受影响区块重建。
    void replaceEntriesAndTriggerRebuild(
        std::vector<ProjEntry>                      newEntries,
        const std::shared_ptr<RenderChunkCoordinator>& coordinator
    );

    // 业务接口：基于当前快照触发重建，不要求调用方接触快照对象。
    void triggerRebuild(const std::shared_ptr<RenderChunkCoordinator>& coordinator) const;

    // 清空所有投影
    void clear();

    // 原子业务接口：清空投影并立刻触发受影响区块重建。
    void clearAndTriggerRebuild(const std::shared_ptr<RenderChunkCoordinator>& coordinator);

    // 单个方块便利函数
    void setSingle(BlockPos pos, const Block* block, mce::Color color);

    // 无锁读取当前快照（工作线程调用）
    std::shared_ptr<const ProjectionSnapshot> getSnapshot() const;

    // 获取当前投影条目数量
    size_t getEntryCount() const;

private:
    // 重建快照（entriesMutex 已锁定时调用）
    void rebuildSnapshot_locked();

    // 活跃快照（atomic shared_ptr，工作线程无锁读取）
    std::atomic<std::shared_ptr<const ProjectionSnapshot>> mSnapshot{
        std::make_shared<const ProjectionSnapshot>()
    };

    // 原始条目（主线程修改，mutex 保护）
    std::vector<ProjEntry> mEntries;
    mutable std::mutex     mEntriesMutex;
};

// ================================================================
// 全局单例访问
// ================================================================
ProjectionState& getProjectionState();

// ================================================================
// SubChunk 重建触发
//
// 遍历快照中所有有投影块的 SubChunk，
// 调用 RenderChunkCoordinator::_setDirty 触发重建。
// ================================================================
void triggerRebuildForProjection(
    const std::shared_ptr<RenderChunkCoordinator>& coordinator,
    std::shared_ptr<const ProjectionSnapshot>      previousSnapshot = nullptr
);

// ================================================================
// 线程局部状态（供 Hook 使用）
// ================================================================

// 当前 build 持有的快照
extern thread_local std::shared_ptr<const ProjectionSnapshot> tl_currentSnapshot;

// 当前 SubChunk 是否有投影块（false = 快速跳过 tessellateInWorld Hook）
extern thread_local bool tl_hasProjection;

} // namespace levishematic::render
