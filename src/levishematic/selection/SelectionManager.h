#pragma once
// ============================================================
// SelectionManager.h
// 选区管理器（Phase 4 实现）
//
// 管理当前活跃选区：
//   - 两个角点（pos1/pos2）的设置
//   - 选区线框的生成与渲染数据
//   - 选区状态查询
//   - 保存选区内方块为 .mcstructure 文件
// ============================================================

#include "levishematic/selection/AreaSelection.h"
#include "levishematic/selection/Box.h"

#include "mc/world/level/BlockPos.h"

#include <memory>
#include <optional>
#include <string>

// 前向声明
class Dimension;

namespace levishematic::selection {

class SelectionManager {
public:
    struct SelectionWireframeState {
        BlockPos origin;
        BlockPos size;
    };

    static SelectionManager& getInstance();

    // ---- 角点设置 ----
    // 设置角点1（左键点击）
    void setCorner1(const BlockPos& pos);

    // 设置角点2（右键点击）
    void setCorner2(const BlockPos& pos);

    // ---- 状态查询 ----
    bool hasCorner1() const { return mHasCorner1; }
    bool hasCorner2() const { return mHasCorner2; }
    bool hasCompleteSelection() const { return mHasCorner1 && mHasCorner2; }

    const BlockPos& getCorner1() const { return mCorner1; }
    const BlockPos& getCorner2() const { return mCorner2; }

    // 获取选区盒子（如果两个角点都已设置）
    std::optional<Box> getSelectionBox() const;

    // 获取选区最小角和尺寸
    BlockPos getMinCorner() const;
    BlockPos getSize() const;

    // ---- 选区操作 ----
    // 清除选区
    void clearSelection();

    // ---- 选区线框状态 ----
    bool                           hasSelectionWireframe() const { return mWireframeState.has_value(); }
    std::optional<SelectionWireframeState> getSelectionWireframeState() const { return mWireframeState; }

    // ---- 保存功能 ----
    // 将选区内方块保存为 .mcstructure 文件
    // name: 文件名（不含扩展名）
    // dimension: 当前维度
    // 返回: 是否保存成功
    bool saveToMcstructure(const std::string& name, Dimension& dimension);

    // ---- 选区模式 ----
    bool isSelectionMode() const { return mSelectionMode; }
    void setSelectionMode(bool enabled) { mSelectionMode = enabled; }
    void toggleSelectionMode() { mSelectionMode = !mSelectionMode; }

private:
    SelectionManager() = default;

    void refreshWireframeStateFromSelection();

    BlockPos mCorner1;
    BlockPos mCorner2;
    bool     mHasCorner1     = false;
    bool     mHasCorner2     = false;
    bool     mSelectionMode  = true; // 默认开启选区模式
    std::optional<SelectionWireframeState> mWireframeState;
};

} // namespace levishematic::selection
