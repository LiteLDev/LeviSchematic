#include "PlacementRepository.h"

#include <algorithm>

namespace levishematic::placement {

SchematicPlacement::Id PlacementRepository::add(SchematicPlacement placement) {
    auto id = placement.getId();
    mPlacements.push_back(std::move(placement));
    mSelectedId      = id;
    mProjectionDirty = true;
    return id;
}

bool PlacementRepository::remove(SchematicPlacement::Id id) {
    auto it = std::find_if(mPlacements.begin(), mPlacements.end(), [id](const SchematicPlacement& placement) {
        return placement.getId() == id;
    });
    if (it == mPlacements.end()) {
        return false;
    }

    mPlacements.erase(it);
    if (mSelectedId == id) {
        selectLast();
    }
    mProjectionDirty = true;
    return true;
}

void PlacementRepository::clear() {
    mPlacements.clear();
    mSelectedId      = 0;
    mProjectionDirty = true;
}

SchematicPlacement* PlacementRepository::getPlacement(SchematicPlacement::Id id) {
    auto it = std::find_if(mPlacements.begin(), mPlacements.end(), [id](const SchematicPlacement& placement) {
        return placement.getId() == id;
    });
    return it == mPlacements.end() ? nullptr : &(*it);
}

const SchematicPlacement* PlacementRepository::getPlacement(SchematicPlacement::Id id) const {
    auto it = std::find_if(mPlacements.begin(), mPlacements.end(), [id](const SchematicPlacement& placement) {
        return placement.getId() == id;
    });
    return it == mPlacements.end() ? nullptr : &(*it);
}

SchematicPlacement* PlacementRepository::getSelected() {
    return getPlacement(mSelectedId);
}

const SchematicPlacement* PlacementRepository::getSelected() const {
    return getPlacement(mSelectedId);
}

void PlacementRepository::selectLast() {
    mSelectedId = mPlacements.empty() ? 0 : mPlacements.back().getId();
}

bool PlacementRepository::needsProjectionRefresh() const {
    if (mProjectionDirty) {
        return true;
    }
    for (const auto& placement : mPlacements) {
        if (placement.isDirty()) {
            return true;
        }
    }
    return false;
}

void PlacementRepository::clearProjectionDirty() {
    mProjectionDirty = false;
}

void PlacementRepository::clearPlacementDirty() {
    for (auto& placement : mPlacements) {
        placement.clearDirty();
    }
}

} // namespace levishematic::placement

