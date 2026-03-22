#include "DataManager.h"

#include "levishematic/render/ProjectionRenderer.h"

#include <filesystem>

namespace levishematic::core {

DataManager& DataManager::getInstance() {
    static DataManager instance;
    return instance;
}

render::ProjectionState& DataManager::getProjectionState() {
    return render::getProjectionState();
}

void DataManager::triggerRebuild(std::shared_ptr<RenderChunkCoordinator> coordinator) {
    render::triggerRebuildForProjection(std::move(coordinator));
}

void DataManager::rebuildAndRefresh(std::shared_ptr<RenderChunkCoordinator> coordinator) {
    mPlacementManager.rebuildAndRefresh(std::move(coordinator));
}

void DataManager::init() {
    // 初始化 Schematic 文件搜索目录
    // 默认在工作目录下的 "schematics" 子目录，加载和保存共用
    namespace fs = std::filesystem;
    fs::path schemDir = fs::current_path() / "schematics";
    if (!fs::exists(schemDir)) {
        // 尝试创建目录（静默失败）
        std::error_code ec;
        fs::create_directories(schemDir, ec);
    }
    mPlacementManager.setSchematicDirectory(schemDir);
}

void DataManager::shutdown() {
    // 清空 PlacementManager（含 ProjectionState 清理）
    mPlacementManager.clear();
    // 额外确保 ProjectionState 已清空
    render::getProjectionState().clear();
}

} // namespace levishematic::core
