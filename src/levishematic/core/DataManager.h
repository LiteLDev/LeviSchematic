#pragma once
// ============================================================
// DataManager.h
// 全局状态中枢（单例）
//
// 管理所有子系统：
//   - ProjectionState（投影渲染状态）
//   - PlacementManager（投影放置管理）[Phase 3] ✅
//   - SelectionManager（选区管理）[Phase 4]
//   - ToolMode / LayerRange [Phase 4+]
// ============================================================

#include "levishematic/render/ProjectionRenderer.h"
#include "levishematic/schematic/placement/PlacementManager.h"
#include "levishematic/selection/SelectionManager.h"

#include <memory>

// 前向声明
class RenderChunkCoordinator;

namespace levishematic::core {

// ================================================================
// 操作模式（为 Phase 4+ GUI/热键预留）
// ================================================================
enum class ToolMode {
    NONE,
    PLACEMENT,   // 放置模式（移动/旋转投影）
    SELECTION,   // 选区模式（圈选世界区域）
};

// ================================================================
// 层过滤范围（为 Phase 5+ 验证器预留）
// ================================================================
struct LayerRange {
    int  minY    = -64;
    int  maxY    = 320;
    bool enabled = false;
};

// ================================================================
// DataManager — 全局状态中枢
// ================================================================
class DataManager {
public:
    static DataManager& getInstance();

    // 禁止拷贝/移动
    DataManager(const DataManager&)            = delete;
    DataManager& operator=(const DataManager&) = delete;
    DataManager(DataManager&&)                 = delete;
    DataManager& operator=(DataManager&&)      = delete;

    // ---- 投影渲染状态 ----
    render::ProjectionState& getProjectionState();

    // ---- 放置管理器 [Phase 3] ----
    placement::PlacementManager& getPlacementManager() { return mPlacementManager; }
    const placement::PlacementManager& getPlacementManager() const { return mPlacementManager; }

    // ---- 选区管理器 [Phase 4] ----
    selection::SelectionManager& getSelectionManager() { return selection::SelectionManager::getInstance(); }
    const selection::SelectionManager& getSelectionManager() const { return selection::SelectionManager::getInstance(); }

    // ---- 操作模式 [Phase 4+ 预留] ----
    ToolMode getToolMode() const { return mToolMode; }
    void setToolMode(ToolMode mode) { mToolMode = mode; }

    // ---- 层过滤 [Phase 5+ 预留] ----
    LayerRange& getLayerRange() { return mLayerRange; }
    const LayerRange& getLayerRange() const { return mLayerRange; }

    // ---- 触发重建 ----
    void triggerRebuild(std::shared_ptr<RenderChunkCoordinator> coordinator);

    // ---- 便利方法：重建投影并刷新渲染 ----
    void rebuildAndRefresh(std::shared_ptr<RenderChunkCoordinator> coordinator);

    // ---- 生命周期 ----
    void init();
    void shutdown();

private:
    DataManager() = default;
    ~DataManager() = default;

    placement::PlacementManager mPlacementManager;
    ToolMode                    mToolMode   = ToolMode::NONE;
    LayerRange                  mLayerRange;
};

} // namespace levishematic::core
