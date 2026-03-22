#pragma once
// ============================================================
// SchematicPlacement.h
// 投影放置管理 — 单个 Schematic 的放置实例
//
// 封装：
//   - 指向已加载的 LitematicSchematic（共享指针）
//   - 放置原点（世界坐标）
//   - 主旋转 / 主镜像
//   - 子区域独立变换（SubRegionPlacement）
//   - enabled / renderEnabled 开关
//   - 包围盒计算
//   - 生成 ProjEntry 列表（供 ProjectionState 使用）
//
// Phase 3 核心组件
// ============================================================

#include "levishematic/render/ProjectionRenderer.h"
#include "levishematic/schematic/LitematicSchematic.h"
#include "levishematic/schematic/SchematicTransform.h"

#include "mc/world/level/BlockPos.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace levishematic::placement {

// ================================================================
// 子区域独立放置参数
// ================================================================
struct SubRegionPlacement {
    std::string            name;
    BlockPos               offset;                              // 相对于 Schematic 原点的偏移
    transform::Rotation    rotation = transform::Rotation::NONE;
    transform::Mirror      mirror   = transform::Mirror::NONE;
    bool                   enabled  = true;
};

// ================================================================
// SchematicPlacement — 单个投影放置实例
// ================================================================
class SchematicPlacement {
public:
    struct LocalBlockEntry {
        BlockPos     localPos;
        const Block* block;
    };

    // 唯一 ID（自增）
    using Id = uint32_t;

    SchematicPlacement() = default;

    // 从已加载的 Schematic 创建放置
    SchematicPlacement(
        std::shared_ptr<schematic::LitematicSchematic> schematic,
        BlockPos                                        origin,
        const std::string&                              name = ""
    );

    SchematicPlacement(
        std::vector<LocalBlockEntry> localBlocks,
        BlockPos                    size,
        BlockPos                    origin,
        const std::string&          name = ""
    );

    // ---- 基本信息 ----
    Id                  getId()       const { return mId; }
    const std::string&  getName()     const { return mName; }
    const std::string&  getFilePath() const { return mFilePath; }
    void                setName(const std::string& name) { mName = name; }
    void                setFilePath(const std::string& path) { mFilePath = path; }

    // ---- 开关 ----
    bool isEnabled()       const { return mEnabled; }
    bool isRenderEnabled() const { return mRenderEnabled; }
    void setEnabled(bool v)       { mEnabled = v; markDirty(); }
    void setRenderEnabled(bool v) { mRenderEnabled = v; markDirty(); }
    void toggleEnabled()          { mEnabled = !mEnabled; markDirty(); }
    void toggleRender()           { mRenderEnabled = !mRenderEnabled; markDirty(); }

    // ---- 变换操作 ----
    BlockPos            getOrigin()   const { return mOrigin; }
    transform::Rotation getRotation() const { return mRotation; }
    transform::Mirror   getMirror()   const { return mMirror; }

    void setOrigin(BlockPos origin);
    void move(int dx, int dy, int dz);
    void setRotation(transform::Rotation rot);
    void rotateCW90();
    void rotateCCW90();
    void rotate180();
    void setMirror(transform::Mirror mir);
    void toggleMirrorX();
    void toggleMirrorZ();
    void resetTransform();

    // ---- 子区域管理 ----
    const std::unordered_map<std::string, SubRegionPlacement>& getSubRegions() const { return mSubRegions; }
    SubRegionPlacement* getSubRegion(const std::string& name);
    void setSubRegionEnabled(const std::string& name, bool enabled);
    void setSubRegionTransform(const std::string& name, transform::Rotation rot, transform::Mirror mir);

    // ---- Schematic 访问 ----
    std::shared_ptr<schematic::LitematicSchematic> getSchematic() const { return mSchematic; }
    bool hasSchematic() const { return mSchematic != nullptr && mSchematic->isLoaded(); }
    bool hasLocalBlocks() const { return !mLocalBlocks.empty(); }

    // ---- 投影块生成 ----
    // 生成 ProjEntry 列表（世界绝对坐标），skipAir = true 跳过空气块
    // 使用默认投影颜色
    std::vector<render::ProjEntry> generateProjEntries(bool skipAir = true) const;

    // 带自定义颜色
    std::vector<render::ProjEntry> generateProjEntries(mce::Color color, bool skipAir = true) const;

    // ---- 包围盒 ----
    // 计算放置后的世界坐标包围盒（minPos, maxPos）
    std::pair<BlockPos, BlockPos> getEnclosingBox() const;

    // ---- 脏标志（用于缓存失效）----
    bool isDirty() const { return mDirty; }
    void clearDirty()    { mDirty = false; }

    // ---- 变换描述 ----
    std::string describeTransform() const;

    // ---- 统计信息 ----
    int getTotalNonAirBlocks() const;

private:
    void markDirty() { mDirty = true; }
    void initSubRegions();

    // ---- 数据成员 ----
    static Id sNextId;

    Id          mId            = 0;
    std::string mName;
    std::string mFilePath;

    std::shared_ptr<schematic::LitematicSchematic> mSchematic;
    std::vector<LocalBlockEntry>                mLocalBlocks;
    std::optional<BlockPos>                     mLocalSize;
    BlockPos                                    mOrigin   = {0, 0, 0};
    transform::Rotation                         mRotation = transform::Rotation::NONE;
    transform::Mirror                           mMirror   = transform::Mirror::NONE;

    bool mEnabled       = true;
    bool mRenderEnabled = true;
    bool mDirty         = true;

    // 子区域名 → 子区域放置参数
    std::unordered_map<std::string, SubRegionPlacement> mSubRegions;
};

} // namespace levishematic::placement
