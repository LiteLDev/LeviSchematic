#pragma once

#include "levishematic/schematic/placement/PlacementProjectionService.h"
#include "levishematic/schematic/placement/PlacementRepository.h"
#include "levishematic/schematic/placement/PlacementTypes.h"
#include "levishematic/schematic/placement/SchematicLoader.h"

#include "mc/world/level/BlockPos.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class RenderChunkCoordinator;

namespace levishematic::placement {

class PlacementManager {
public:
    PlacementManager() = default;

    SchematicPlacement::Id loadAndPlace(
        const std::filesystem::path& path,
        BlockPos                      origin,
        const std::string&            name = ""
    );
    LoadPlacementResult loadAndPlaceDetailed(
        const std::filesystem::path& path,
        BlockPos                      origin,
        const std::string&            name = ""
    );

    bool removePlacement(SchematicPlacement::Id id);
    void removeAllPlacements();

    SchematicPlacement*       getPlacement(SchematicPlacement::Id id);
    const SchematicPlacement* getPlacement(SchematicPlacement::Id id) const;

    const std::vector<SchematicPlacement>& getAllPlacements() const { return mRepository.getAllPlacements(); }
    size_t                                 getPlacementCount() const { return mRepository.getPlacementCount(); }
    bool                                   isEmpty() const { return mRepository.isEmpty(); }

    SchematicPlacement::Id getSelectedId() const { return mRepository.getSelectedId(); }
    void                   setSelectedId(SchematicPlacement::Id id);
    SchematicPlacement*    getSelected();
    const SchematicPlacement* getSelected() const;
    void                   selectLast();

    void rebuildProjection();
    void rebuildAndRefresh(std::shared_ptr<RenderChunkCoordinator> coordinator);
    bool needsProjectionRefresh() const;

    using OnChangeCallback = std::function<void()>;
    void setOnChangeCallback(OnChangeCallback cb) { mOnChange = std::move(cb); }

    void clear();

    void                          setSchematicDirectory(const std::filesystem::path& dir) { mSchematicDir = dir; }
    const std::filesystem::path&  getSchematicDirectory() const { return mSchematicDir; }
    std::filesystem::path         resolveSchematicPath(const std::string& filename) const;
    std::vector<std::string>      listAvailableFiles() const;

private:
    void notifyChange();
    void clearDirtyState();

    PlacementRepository        mRepository;
    SchematicLoader            mLoader;
    PlacementProjectionService mProjectionService;
    std::filesystem::path      mSchematicDir;
    OnChangeCallback           mOnChange;
};

} // namespace levishematic::placement
