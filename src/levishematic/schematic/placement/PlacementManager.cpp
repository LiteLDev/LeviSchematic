#include "PlacementManager.h"

#include "levishematic/LeviShematic.h"
#include "levishematic/core/DataManager.h"
#include "levishematic/schematic/SchematicPathResolver.h"

#include <algorithm>

namespace levishematic::placement {
namespace {

auto& getLogger() {
    return levishematic::LeviShematic::getInstance().getSelf().getLogger();
}

std::filesystem::path makeAbsoluteIfPossible(const std::filesystem::path& path) {
    std::error_code ec;
    auto            absolute = std::filesystem::absolute(path, ec);
    return ec ? path : absolute;
}

} // namespace

SchematicPlacement::Id PlacementManager::loadAndPlace(
    const std::filesystem::path& path,
    BlockPos                      origin,
    const std::string&            name
) {
    return loadAndPlaceDetailed(path, origin, name).id;
}

LoadPlacementResult PlacementManager::loadAndPlaceDetailed(
    const std::filesystem::path& path,
    BlockPos                      origin,
    const std::string&            name
) {
    auto placementResult = mLoader.loadMcstructurePlacement(path, origin, name);
    if (!placementResult) {
        return LoadPlacementResult{
            .resolvedPath = path,
            .error        = placementResult.error(),
        };
    }

    auto placement    = std::move(placementResult).value();
    auto absolutePath = makeAbsoluteIfPossible(path);
    placement.setFilePath(absolutePath.string());

    auto id = mRepository.add(std::move(placement));
    notifyChange();
    return LoadPlacementResult{
        .id           = id,
        .resolvedPath = absolutePath,
    };
}

bool PlacementManager::removePlacement(SchematicPlacement::Id id) {
    if (!mRepository.remove(id)) {
        getLogger().warn(
            "Placement operation failed [operation={}, file={}, placementId={}]: placement not found",
            "removePlacement",
            "",
            id
        );
        return false;
    }

    notifyChange();
    return true;
}

void PlacementManager::removeAllPlacements() {
    mRepository.clear();
    notifyChange();
}

SchematicPlacement* PlacementManager::getPlacement(SchematicPlacement::Id id) {
    return mRepository.getPlacement(id);
}

const SchematicPlacement* PlacementManager::getPlacement(SchematicPlacement::Id id) const {
    return mRepository.getPlacement(id);
}

void PlacementManager::setSelectedId(SchematicPlacement::Id id) {
    mRepository.setSelectedId(id);
    notifyChange();
}

SchematicPlacement* PlacementManager::getSelected() {
    return mRepository.getSelected();
}

const SchematicPlacement* PlacementManager::getSelected() const {
    return mRepository.getSelected();
}

void PlacementManager::selectLast() {
    mRepository.selectLast();
    notifyChange();
}

void PlacementManager::rebuildProjection() {
    mProjectionService.rebuildProjection(mRepository);
    clearDirtyState();
}

void PlacementManager::rebuildAndRefresh(std::shared_ptr<RenderChunkCoordinator> coordinator) {
    mProjectionService.rebuildAndRefresh(mRepository, coordinator);
    clearDirtyState();
}

bool PlacementManager::needsProjectionRefresh() const {
    return mRepository.needsProjectionRefresh();
}

std::filesystem::path PlacementManager::resolveSchematicPath(const std::string& filename) const {
    auto schematicDir = mSchematicDir.empty()
        ? core::DataManager::getInstance().getSchematicDirectory()
        : mSchematicDir;

    return schematic::SchematicPathResolver(schematicDir).resolveExistingPath(filename);
}

std::vector<std::string> PlacementManager::listAvailableFiles() const {
    namespace fs = std::filesystem;

    std::vector<std::string> files;
    auto schematicDir = mSchematicDir.empty()
        ? core::DataManager::getInstance().getSchematicDirectory()
        : mSchematicDir;
    if (schematicDir.empty() || !fs::is_directory(schematicDir)) {
        return files;
    }

    for (const auto& entry : fs::directory_iterator(schematicDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == ".mcstructure") {
            files.push_back(entry.path().filename().string());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

void PlacementManager::clear() {
    mRepository.clear();
    mProjectionService.clearProjection();
    clearDirtyState();
}

void PlacementManager::notifyChange() {
    if (mOnChange) {
        mOnChange();
    }
}

void PlacementManager::clearDirtyState() {
    mRepository.clearProjectionDirty();
    mRepository.clearPlacementDirty();
}

} // namespace levishematic::placement

