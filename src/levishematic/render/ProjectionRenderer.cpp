#include "ProjectionRenderer.h"

#include "levishematic/util/PositionUtils.h"

#include "mc/client/renderer/chunks/RenderChunkCoordinator.h"

namespace levishematic::render {

// ================================================================
// 线程局部状态定义
// ================================================================
thread_local std::shared_ptr<const ProjectionSnapshot> tl_currentSnapshot;
thread_local bool                                      tl_hasProjection = false;

// ================================================================
// ProjectionState 实现
// ================================================================

void ProjectionState::setEntries(std::vector<ProjEntry> newEntries) {
    replaceEntries(std::move(newEntries));
}

std::shared_ptr<const ProjectionSnapshot> ProjectionState::replaceEntries(std::vector<ProjEntry> newEntries) {
    std::lock_guard<std::mutex> lk(mEntriesMutex);
    auto previous = mSnapshot.load(std::memory_order_acquire);
    mEntries = std::move(newEntries);
    rebuildSnapshot_locked();
    return previous;
}

void ProjectionState::clear() { setEntries({}); }

void ProjectionState::setSingle(BlockPos pos, const Block* block, mce::Color color) {
    setEntries({ProjEntry{pos, block, color}});
}

std::shared_ptr<const ProjectionSnapshot> ProjectionState::getSnapshot() const {
    return mSnapshot.load(std::memory_order_acquire);
}

size_t ProjectionState::getEntryCount() const {
    std::lock_guard<std::mutex> lk(mEntriesMutex);
    return mEntries.size();
}

void ProjectionState::rebuildSnapshot_locked() {
    auto snap = std::make_shared<ProjectionSnapshot>();
    for (const auto& e : mEntries) {
        uint64_t scKey = util::subChunkKeyFromWorldPos(e.pos.x, e.pos.y, e.pos.z);
        snap->bySubChunk[scKey].push_back(e);
        snap->posColorMap[util::encodePosKey(e.pos)] = e.color;
    }
    mSnapshot.store(std::move(snap), std::memory_order_release);
}

// ================================================================
// 全局单例
// ================================================================
static ProjectionState gProjectionState;

ProjectionState& getProjectionState() { return gProjectionState; }

// ================================================================
// SubChunk 重建触发
//
// RenderChunkCoordinator::_setDirty（RVA: 0x201fd00）：
//   void _setDirty(const BlockPos& min, const BlockPos& max,
//                  bool immediate, bool changesVisibility, bool canInterlock)
//
//   min/max 是世界坐标，内部 >>4 转 SubChunk 索引。
//   immediate=false → 下一帧重建（推荐，避免卡顿）
// ================================================================

void triggerRebuildForProjection(
    const std::shared_ptr<RenderChunkCoordinator>& coordinator,
    std::shared_ptr<const ProjectionSnapshot>      previousSnapshot
) {
    if (!coordinator) return;

    auto currentSnapshot = gProjectionState.getSnapshot();
    std::unordered_set<uint64_t> dirtyKeys;

    auto markSnapshot = [&](const std::shared_ptr<const ProjectionSnapshot>& snapshot) {
        if (!snapshot) return;
        for (const auto& [scKey, vec] : snapshot->bySubChunk) {
            if (!vec.empty()) {
                dirtyKeys.insert(scKey);
            }
        }
    };

    markSnapshot(previousSnapshot);
    markSnapshot(currentSnapshot);

    if (dirtyKeys.empty()) return;

    auto setDirtyForSnapshot = [&](const std::shared_ptr<const ProjectionSnapshot>& snapshot) {
        if (!snapshot) return;
        for (const auto& [scKey, vec] : snapshot->bySubChunk) {
            if (dirtyKeys.erase(scKey) == 0 || vec.empty()) continue;
            const BlockPos& p = vec[0].pos;
            coordinator->_setDirty(p, p, false, false, false);
        }
    };

    setDirtyForSnapshot(previousSnapshot);
    setDirtyForSnapshot(currentSnapshot);

    // 极少数情况下兜底，避免因为快照异常导致旧区块没被重建。
    for (const auto& scKey : dirtyKeys) {
        int sx = static_cast<int>(scKey >> 42);
        int sz = static_cast<int>((scKey >> 21) & 0x1FFFFFu);
        int sy = static_cast<int>(scKey & 0x1FFFFFu);

        if (sx & (1 << 21)) sx |= ~0x1FFFFF;
        if (sz & (1 << 20)) sz |= ~0x1FFFFF;
        if (sy & (1 << 20)) sy |= ~0x1FFFFF;

        BlockPos p{sx << 4, sy << 4, sz << 4};
        coordinator->_setDirty(p, p, false, false, false);
    }
}

} // namespace levishematic::render
