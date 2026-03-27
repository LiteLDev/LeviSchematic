#pragma once

#include "levishematic/schematic/placement/SchematicPlacement.h"

#include <vector>

namespace levishematic::placement {

class PlacementRepository {
public:
    SchematicPlacement::Id add(SchematicPlacement placement);
    bool                   remove(SchematicPlacement::Id id);
    void                   clear();

    SchematicPlacement*       getPlacement(SchematicPlacement::Id id);
    const SchematicPlacement* getPlacement(SchematicPlacement::Id id) const;

    const std::vector<SchematicPlacement>& getAllPlacements() const { return mPlacements; }
    size_t                                 getPlacementCount() const { return mPlacements.size(); }
    bool                                   isEmpty() const { return mPlacements.empty(); }

    SchematicPlacement::Id    getSelectedId() const { return mSelectedId; }
    void                      setSelectedId(SchematicPlacement::Id id) { mSelectedId = id; }
    SchematicPlacement*       getSelected();
    const SchematicPlacement* getSelected() const;
    void                      selectLast();

    bool needsProjectionRefresh() const;
    void clearProjectionDirty();
    void clearPlacementDirty();

private:
    std::vector<SchematicPlacement> mPlacements;
    SchematicPlacement::Id          mSelectedId      = 0;
    bool                            mProjectionDirty = false;
};

} // namespace levishematic::placement

