#include "DataManager.h"

#include "levishematic/render/ProjectionRenderer.h"

#include "ll/api/service/ServerInfo.h"

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
    getProjectionState().triggerRebuild(std::move(coordinator));
}

void DataManager::rebuildAndRefresh(std::shared_ptr<RenderChunkCoordinator> coordinator) {
    mPlacementManager.rebuildAndRefresh(std::move(coordinator));
    mPlacementRefreshPending = false;
}

void DataManager::requestPlacementRefresh() {
    mPlacementRefreshPending = true;
}

bool DataManager::flushPlacementRefresh(std::shared_ptr<RenderChunkCoordinator> coordinator) {
    if (!coordinator) {
        return false;
    }
    if (!(mPlacementRefreshPending || mPlacementManager.needsProjectionRefresh())) {
        return false;
    }

    mPlacementManager.rebuildAndRefresh(std::move(coordinator));
    mPlacementRefreshPending = false;
    return true;
}

const std::filesystem::path& DataManager::getSchematicDirectory() const {
    return mSchematicDirectory;
}

std::filesystem::path DataManager::ensureSchematicDirectory() {
    std::error_code ec;
    std::filesystem::create_directories(mSchematicDirectory, ec);
    return mSchematicDirectory;
}

schematic::SchematicPathResolver DataManager::createSchematicPathResolver() const {
    return schematic::SchematicPathResolver(mSchematicDirectory);
}

std::filesystem::path DataManager::makeSchematicFilePath(const std::filesystem::path& path) const {
    return createSchematicPathResolver().makeFilePath(path);
}

void DataManager::init() {
    std::filesystem::path structurePath;
    structurePath /= ll::getWorldPath().value();
    structurePath = structurePath.parent_path().parent_path();
    structurePath /= "schematics";

    mSchematicDirectory = structurePath;
    mPlacementManager.setSchematicDirectory(ensureSchematicDirectory());
    mPlacementManager.setOnChangeCallback([this]() { requestPlacementRefresh(); });
}

void DataManager::shutdown() {
    mPlacementManager.clear();
    render::getProjectionState().clear();
    mPlacementRefreshPending = false;
}

} // namespace levishematic::core
