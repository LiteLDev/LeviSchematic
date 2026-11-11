#pragma once

#include "levischematic/schematic/placement/PlacementModel.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace levischematic::placement {
struct PatchOp;
}

namespace levischematic::placement {





struct PlacementState {
    std::unordered_map<PlacementId, PlacementInstance> placements;
    std::vector<PlacementId>                           order;
    std::optional<PlacementId>                         selectedId;
    uint64_t                                           revision = 0;
    PlacementId                                        nextPlacementId = 1;
    mutable std::recursive_mutex                       mutex;
    mutable uint64_t projectionRevision = UINT64_MAX;
    mutable std::unordered_map<PlacementId, PlacementInstance> projections;
    mutable std::vector<PlacementId> projectionOrder;
    mutable std::unordered_map<std::string, PlacementId> projectionIds;
    mutable PlacementId nextProjectionId = 0x80000000u;
};





class PlacementStore {
public:
    



    explicit PlacementStore(PlacementState& state) : mState(state) {}

    


    PlacementState&       state() { return mState; }
    


    PlacementState const& state() const { return mState; }

    







    PlacementId createPlacement(
        std::shared_ptr<const SchematicAsset> asset,
        BlockPos                              origin,
        int                                   dimensionId,
        std::string                           name,
        std::filesystem::path                 filePath
    );

    



    PlacementInstance*       get(PlacementId id);
    



    PlacementInstance const* get(PlacementId id) const;
    


    PlacementInstance*       selected();
    


    PlacementInstance const* selected() const;

    



    [[nodiscard]] bool remove(PlacementId id);
    


    void               clear();
    
    void replaceDimension(int dimension, std::vector<PlacementInstance> restored, std::optional<size_t> selected);
    



    [[nodiscard]] bool select(PlacementId id);
    void               deselect();
    


    void               selectLast();

    






    [[nodiscard]] bool move(PlacementId id, int dx, int dy, int dz);
    




    [[nodiscard]] bool setOrigin(PlacementId id, BlockPos origin);
    




    [[nodiscard]] bool rotate(PlacementId id, PlacementInstance::Rotation delta);
    




    [[nodiscard]] bool setMirror(PlacementId id, PlacementInstance::Mirror mirror);
    



    [[nodiscard]] bool resetTransform(PlacementId id);
    



    [[nodiscard]] bool toggleEnabled(PlacementId id);
    



    [[nodiscard]] bool toggleRender(PlacementId id);
    





    [[nodiscard]] bool patchBlock(PlacementId id, BlockPos worldPos, placement::PatchOp const& op);

    bool rename(PlacementId id, std::string name);
    bool setLocks(PlacementId id, bool locked, std::array<bool, 3> axes);
    bool setEntitiesEnabled(PlacementId id, bool enabled);
    bool setGrid(PlacementId, GridSettings);
    bool setRegion(PlacementId, std::string const&, RegionSettings);
    bool bindAsset(PlacementId, std::shared_ptr<const SchematicAsset>, std::filesystem::path);
    void markResourceChange() {std::scoped_lock lock(mState.mutex);touchState();}
    void replaceAsset(std::shared_ptr<const SchematicAsset> const& previous,
                      std::shared_ptr<const SchematicAsset> next);

private:
    bool commitTransform(PlacementInstance&, BlockPos, PlacementInstance::Rotation,
                         PlacementInstance::Mirror, bool contentChanged);
    


    void touchState();
    



    void touchPlacement(PlacementInstance& placement, bool contentChanged = true);

    PlacementState& mState;
};

} 
