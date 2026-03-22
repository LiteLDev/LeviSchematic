#include "PlacementManager.h"

#include "levishematic/core/DataManager.h"
#include "levishematic/schematic/NBTReader.h"
#include "levishematic/render/ProjectionRenderer.h"
#include "levishematic/schematic/LitematicSchematic.h"

#include "ll/api/service/Bedrock.h"

#include "mc/nbt/CompoundTag.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"

#include <algorithm>
#include <fstream>

namespace levishematic::placement {

// ================================================================
// SchematicHolder 实现
// ================================================================

std::shared_ptr<schematic::LitematicSchematic>
SchematicHolder::loadSchematic(const std::filesystem::path& path) {
    // 规范化路径作为 key
    std::string key = std::filesystem::absolute(path).string();

    // 检查缓存
    auto it = mCache.find(key);
    if (it != mCache.end() && it->second && it->second->isLoaded()) {
        return it->second;
    }

    // 加载新的 Schematic
    auto schem = std::make_shared<schematic::LitematicSchematic>();
    if (!schem->loadFromFile(path)) {
        return nullptr; // 加载失败
    }

    mCache[key] = schem;
    return schem;
}

std::shared_ptr<schematic::LitematicSchematic>
SchematicHolder::getSchematic(const std::string& key) const {
    auto it = mCache.find(key);
    return (it != mCache.end()) ? it->second : nullptr;
}

bool SchematicHolder::hasSchematic(const std::string& key) const {
    return mCache.find(key) != mCache.end();
}

void SchematicHolder::cleanup() {
    for (auto it = mCache.begin(); it != mCache.end(); ) {
        // use_count == 1 表示只有缓存自身持有引用
        if (it->second.use_count() <= 1) {
            it = mCache.erase(it);
        } else {
            ++it;
        }
    }
}

void SchematicHolder::clear() {
    mCache.clear();
}

std::vector<std::string> SchematicHolder::getCachedNames() const {
    std::vector<std::string> names;
    names.reserve(mCache.size());
    for (const auto& [key, schem] : mCache) {
        if (schem && schem->isLoaded()) {
            names.push_back(schem->getMetadata().name);
        }
    }
    return names;
}

// ================================================================
// PlacementManager 实现
// ================================================================

SchematicPlacement::Id PlacementManager::loadAndPlace(
    const std::filesystem::path& path,
    BlockPos                      origin,
    const std::string&            name
) {
    if (path.extension() == ".mcstructure") {
        auto placement = loadMcstructurePlacement(path, origin, name);
        if (!placement.has_value()) {
            return 0;
        }

        auto id = placement->getId();
        placement->setFilePath(std::filesystem::absolute(path).string());
        mPlacements.push_back(std::move(*placement));
        mSelectedId = id;
        notifyChange();
        return id;
    }

    // 尝试加载 Schematic（优先从缓存获取）
    auto schem = mHolder.loadSchematic(path);
    if (!schem) {
        return 0; // 加载失败
    }

    // 创建 Placement
    SchematicPlacement placement(schem, origin, name);
    placement.setFilePath(std::filesystem::absolute(path).string());

    SchematicPlacement::Id id = placement.getId();
    mPlacements.push_back(std::move(placement));
    mSelectedId = id; // 自动选中新创建的

    notifyChange();
    return id;
}

SchematicPlacement::Id PlacementManager::addPlacement(
    std::shared_ptr<schematic::LitematicSchematic> schematic,
    BlockPos                                        origin,
    const std::string&                              name
) {
    if (!schematic || !schematic->isLoaded()) {
        return 0;
    }

    SchematicPlacement placement(std::move(schematic), origin, name);
    SchematicPlacement::Id id = placement.getId();
    mPlacements.push_back(std::move(placement));
    mSelectedId = id;

    notifyChange();
    return id;
}

bool PlacementManager::removePlacement(SchematicPlacement::Id id) {
    auto it = std::find_if(mPlacements.begin(), mPlacements.end(),
        [id](const SchematicPlacement& p) { return p.getId() == id; });

    if (it == mPlacements.end()) return false;

    mPlacements.erase(it);

    // 如果删除的是当前选中的，自动选中最后一个
    if (mSelectedId == id) {
        if (!mPlacements.empty()) {
            mSelectedId = mPlacements.back().getId();
        } else {
            mSelectedId = 0;
        }
    }

    notifyChange();
    return true;
}

void PlacementManager::removeAllPlacements() {
    mPlacements.clear();
    mSelectedId = 0;
    notifyChange();
}

SchematicPlacement* PlacementManager::getPlacement(SchematicPlacement::Id id) {
    auto it = std::find_if(mPlacements.begin(), mPlacements.end(),
        [id](const SchematicPlacement& p) { return p.getId() == id; });
    return (it != mPlacements.end()) ? &(*it) : nullptr;
}

const SchematicPlacement* PlacementManager::getPlacement(SchematicPlacement::Id id) const {
    auto it = std::find_if(mPlacements.begin(), mPlacements.end(),
        [id](const SchematicPlacement& p) { return p.getId() == id; });
    return (it != mPlacements.end()) ? &(*it) : nullptr;
}

SchematicPlacement* PlacementManager::getSelected() {
    return getPlacement(mSelectedId);
}

const SchematicPlacement* PlacementManager::getSelected() const {
    return getPlacement(mSelectedId);
}

void PlacementManager::selectLast() {
    if (!mPlacements.empty()) {
        mSelectedId = mPlacements.back().getId();
    } else {
        mSelectedId = 0;
    }
}

// ================================================================
// 投影状态更新
// ================================================================

void PlacementManager::rebuildProjection() {
    std::vector<render::ProjEntry> allEntries;

    for (const auto& placement : mPlacements) {
        if (!placement.isEnabled() || !placement.isRenderEnabled()) continue;

        auto entries = placement.generateProjEntries();
        allEntries.insert(allEntries.end(),
                          std::make_move_iterator(entries.begin()),
                          std::make_move_iterator(entries.end()));
    }

    // 更新全局 ProjectionState
    render::getProjectionState().setEntries(std::move(allEntries));
}

void PlacementManager::rebuildAndRefresh(std::shared_ptr<RenderChunkCoordinator> coordinator) {
    auto previousSnapshot = render::getProjectionState().getSnapshot();
    rebuildProjection();
    render::triggerRebuildForProjection(std::move(coordinator), std::move(previousSnapshot));
}

// ================================================================
// Schematic 文件路径解析
// ================================================================

std::filesystem::path PlacementManager::resolveSchematicPath(const std::string& filename) const {
    namespace fs = std::filesystem;

    // 1. 如果是绝对路径且存在，直接返回
    fs::path p(filename);
    if (p.is_absolute() && fs::exists(p)) {
        return p;
    }

    // 2. 在 schematic 目录中查找
    if (!mSchematicDir.empty()) {
        fs::path inDir = mSchematicDir / filename;
        if (fs::exists(inDir)) return inDir;

        // 尝试添加已支持的扩展名
        fs::path withExt = mSchematicDir / (filename + ".litematic");
        if (fs::exists(withExt)) return withExt;
        withExt = mSchematicDir / (filename + ".mcstructure");
        if (fs::exists(withExt)) return withExt;
    }

    // 3. 在当前目录查找
    if (fs::exists(p)) return fs::absolute(p);

    // 4. 尝试添加扩展名
    fs::path withExt(filename + ".litematic");
    if (fs::exists(withExt)) return fs::absolute(withExt);
    withExt = fs::path(filename + ".mcstructure");
    if (fs::exists(withExt)) return fs::absolute(withExt);

    // 未找到，返回原始路径（让调用方处理错误）
    return p;
}

std::vector<std::string> PlacementManager::listAvailableFiles() const {
    namespace fs = std::filesystem;
    std::vector<std::string> files;

    if (mSchematicDir.empty() || !fs::is_directory(mSchematicDir)) {
        return files;
    }

    for (const auto& entry : fs::directory_iterator(mSchematicDir)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext == ".litematic" || ext == ".mcstructure") {
            files.push_back(entry.path().filename().string());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

// ================================================================
// 生命周期
// ================================================================

void PlacementManager::clear() {
    mPlacements.clear();
    mSelectedId = 0;
    mHolder.clear();
    // 清空 ProjectionState
    render::getProjectionState().clear();
}

void PlacementManager::notifyChange() {
    if (mOnChange) {
        mOnChange();
    }
}

std::optional<SchematicPlacement> PlacementManager::loadMcstructurePlacement(
    const std::filesystem::path& path,
    BlockPos                      origin,
    const std::string&            name
) const {
    try {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return std::nullopt;
        }

        auto fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        std::string rawData(fileSize, '\0');
        if (!file.read(rawData.data(), static_cast<std::streamsize>(fileSize))) {
            return std::nullopt;
        }

        auto tagResult = CompoundTag::fromBinaryNbt(rawData, true);
        if (!tagResult) {
            return std::nullopt;
        }

        auto registry = ll::service::getLevel()->getUnknownBlockTypeRegistry();
        StructureTemplate structureTemplate(path.filename().string(), registry);
        if (!structureTemplate.load(*tagResult)) {
            return std::nullopt;
        }

        const auto& data = structureTemplate.mStructureTemplateData;
        BlockPos size    = structureTemplate.rawSize();
        if (size.x <= 0 || size.y <= 0 || size.z <= 0) {
            return std::nullopt;
        }

        std::vector<SchematicPlacement::LocalBlockEntry> localBlocks;
        localBlocks.reserve(static_cast<size_t>(size.x) * size.y * size.z);

        for (int y = 0; y < size.y; ++y) {
            for (int z = 0; z < size.z; ++z) {
                for (int x = 0; x < size.x; ++x) {
                    BlockPos localPos{x, y, z};
                    auto* block = StructureTemplate::tryGetBlockAtPos(localPos, data, registry);
                    if (!block || block->isAir()) continue;
                    localBlocks.push_back({localPos, block});
                }
            }
        }

        std::string placementName = name.empty() ? path.stem().string() : name;
        return SchematicPlacement(std::move(localBlocks), size, origin, placementName);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace levishematic::placement
