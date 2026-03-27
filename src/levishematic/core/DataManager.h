#pragma once

#include "levishematic/render/ProjectionRenderer.h"
#include "levishematic/schematic/SchematicPathResolver.h"
#include "levishematic/schematic/placement/PlacementManager.h"
#include "levishematic/selection/SelectionManager.h"

#include <filesystem>
#include <memory>

class RenderChunkCoordinator;

namespace levishematic::core {

enum class ToolMode {
    NONE,
    PLACEMENT,
    SELECTION,
};

struct LayerRange {
    int  minY    = -64;
    int  maxY    = 320;
    bool enabled = false;
};

class DataManager {
public:
    static DataManager& getInstance();

    DataManager(const DataManager&)            = delete;
    DataManager& operator=(const DataManager&) = delete;
    DataManager(DataManager&&)                 = delete;
    DataManager& operator=(DataManager&&)      = delete;

    render::ProjectionState& getProjectionState();

    placement::PlacementManager&       getPlacementManager() { return mPlacementManager; }
    const placement::PlacementManager& getPlacementManager() const { return mPlacementManager; }

    selection::SelectionManager&       getSelectionManager() { return selection::SelectionManager::getInstance(); }
    const selection::SelectionManager& getSelectionManager() const {
        return selection::SelectionManager::getInstance();
    }

    ToolMode getToolMode() const { return mToolMode; }
    void     setToolMode(ToolMode mode) { mToolMode = mode; }

    LayerRange&       getLayerRange() { return mLayerRange; }
    const LayerRange& getLayerRange() const { return mLayerRange; }

    void triggerRebuild(std::shared_ptr<RenderChunkCoordinator> coordinator);
    void rebuildAndRefresh(std::shared_ptr<RenderChunkCoordinator> coordinator);
    void requestPlacementRefresh();
    bool flushPlacementRefresh(std::shared_ptr<RenderChunkCoordinator> coordinator);

    const std::filesystem::path& getSchematicDirectory() const;
    std::filesystem::path        ensureSchematicDirectory();
    schematic::SchematicPathResolver createSchematicPathResolver() const;
    std::filesystem::path        makeSchematicFilePath(const std::filesystem::path& path) const;

    void init();
    void shutdown();

private:
    DataManager()  = default;
    ~DataManager() = default;

    placement::PlacementManager mPlacementManager;
    std::filesystem::path       mSchematicDirectory;
    ToolMode                    mToolMode                = ToolMode::NONE;
    LayerRange                  mLayerRange;
    bool                        mPlacementRefreshPending = false;
};

} // namespace levishematic::core
