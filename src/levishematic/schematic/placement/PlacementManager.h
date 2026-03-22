#pragma once
// ============================================================
// PlacementManager.h
// 投影放置管理器 — 管理所有已加载的投影放置实例
//
// 职责：
//   - 管理多个 SchematicPlacement 实例
//   - 加载/卸载 Schematic 文件 → 创建/删除 Placement
//   - 合并所有 enabled Placement 的 ProjEntry → 更新 ProjectionState
//   - 触发渲染重建
//   - 提供当前选中的 Placement（用于命令操作）
//
// Phase 3 核心组件
// ============================================================

#include "levishematic/schematic/placement/SchematicPlacement.h"
#include "levishematic/render/ProjectionRenderer.h"

#include "mc/world/level/BlockPos.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// 前向声明
class RenderChunkCoordinator;

namespace levishematic::placement {

// ================================================================
// SchematicHolder — Schematic 文件缓存
//
// 同一个 .litematic 文件可以被多个 Placement 引用。
// 使用 shared_ptr 管理生命周期。
// ================================================================
class SchematicHolder {
public:
    // 加载 Schematic 文件（若已缓存则直接返回）
    std::shared_ptr<schematic::LitematicSchematic>
    loadSchematic(const std::filesystem::path& path);

    // 获取已缓存的 Schematic
    std::shared_ptr<schematic::LitematicSchematic>
    getSchematic(const std::string& key) const;

    // 检查缓存
    bool hasSchematic(const std::string& key) const;

    // 清理未被引用的缓存
    void cleanup();

    // 清空所有缓存
    void clear();

    // 获取已缓存的 Schematic 列表（文件名）
    std::vector<std::string> getCachedNames() const;

private:
    // key = 文件绝对路径的字符串
    std::unordered_map<std::string, std::shared_ptr<schematic::LitematicSchematic>> mCache;
};

// ================================================================
// PlacementManager — 投影放置管理器
// ================================================================
class PlacementManager {
public:
    PlacementManager() = default;

    // ---- 加载与创建 ----

    // 从文件加载 Schematic 并在指定位置创建 Placement
    // 返回 Placement ID，失败返回 0
    SchematicPlacement::Id loadAndPlace(
        const std::filesystem::path& path,
        BlockPos                      origin,
        const std::string&            name = ""
    );

    // 从已加载的 Schematic 创建 Placement
    SchematicPlacement::Id addPlacement(
        std::shared_ptr<schematic::LitematicSchematic> schematic,
        BlockPos                                        origin,
        const std::string&                              name = ""
    );

    // ---- 删除 ----
    bool removePlacement(SchematicPlacement::Id id);
    void removeAllPlacements();

    // ---- 查询 ----
    SchematicPlacement*       getPlacement(SchematicPlacement::Id id);
    const SchematicPlacement* getPlacement(SchematicPlacement::Id id) const;

    // 获取所有 Placement
    const std::vector<SchematicPlacement>& getAllPlacements() const { return mPlacements; }
    size_t getPlacementCount() const { return mPlacements.size(); }
    bool   isEmpty() const { return mPlacements.empty(); }

    // ---- 当前选中的 Placement（用于命令快捷操作）----
    SchematicPlacement::Id getSelectedId() const { return mSelectedId; }
    void setSelectedId(SchematicPlacement::Id id) { mSelectedId = id; }

    // 获取当前选中的 Placement（若无选中或已删除，返回 nullptr）
    SchematicPlacement* getSelected();
    const SchematicPlacement* getSelected() const;

    // 自动选中最后添加的 Placement
    void selectLast();

    // ---- 投影状态更新 ----

    // 重建 ProjectionState（合并所有 enabled Placement 的 ProjEntry）
    void rebuildProjection();

    // 重建投影并触发渲染重建
    void rebuildAndRefresh(std::shared_ptr<RenderChunkCoordinator> coordinator);

    // ---- Schematic 缓存访问 ----
    SchematicHolder& getSchematicHolder() { return mHolder; }
    const SchematicHolder& getSchematicHolder() const { return mHolder; }

    // ---- 回调注册（为未来 GUI 预留）----
    using OnChangeCallback = std::function<void()>;
    void setOnChangeCallback(OnChangeCallback cb) { mOnChange = std::move(cb); }

    // ---- 生命周期 ----
    void clear();

    // ---- Schematic 文件搜索路径 ----
    void setSchematicDirectory(const std::filesystem::path& dir) { mSchematicDir = dir; }
    const std::filesystem::path& getSchematicDirectory() const { return mSchematicDir; }

    // 在 schematic 目录中查找文件（支持不带扩展名）
    std::filesystem::path resolveSchematicPath(const std::string& filename) const;

    // 列出 schematic 目录中的所有 .litematic 文件
    std::vector<std::string> listAvailableFiles() const;

private:
    std::optional<SchematicPlacement> loadMcstructurePlacement(
        const std::filesystem::path& path,
        BlockPos                      origin,
        const std::string&            name
    ) const;

    void notifyChange();

    std::vector<SchematicPlacement> mPlacements;
    SchematicPlacement::Id          mSelectedId = 0;
    SchematicHolder                 mHolder;
    std::filesystem::path           mSchematicDir;
    OnChangeCallback                mOnChange;
};

} // namespace levishematic::placement
