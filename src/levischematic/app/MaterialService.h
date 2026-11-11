#pragma once
#include "PlacementService.h"
#include "levischematic/schematic/placement/ProjectionScan.h"
#include "levischematic/survival/BlockItems.h"
#include "levischematic/util/WorkBudget.h"
#include <map>
#include <set>
class LocalPlayer;
namespace levischematic::app {
struct MaterialRow {
    survival::ItemKey        item;
    std::string              label;
    survival::MaterialCounts counts;
};
struct MaterialReport {
    uint32_t                 placementId = 0;
    bool                     building = false, stale = true, layerOnly = false, secondLayer = false;
    size_t                   scanned = 0, total = 0, unresolved = 0;
    std::vector<MaterialRow> rows;
    std::vector<std::string> warnings;
};

class MaterialService {
public:
    explicit MaterialService(PlacementService& placements) : mPlacements(placements) {}
    void           request(bool layerOnly);
    void           invalidate();
    void           tick(LocalPlayer&, util::WorkBudget const&);
    MaterialReport report() const { return mPublished; }
    std::string    exportCsv(std::filesystem::path const& directory) const;

private:
    void              accumulate(placement::PlacementInstance const&, placement::ExpectedPosition const&, LocalPlayer&);
    PlacementService& mPlacements;
    placement::ProjectionScan mScan;
    MaterialReport    mPublished, mWorking;
    std::map<survival::ItemKey, MaterialRow> mRows;
    std::set<std::string>                    mWarnings;
    std::set<uint64_t>                       mUnits;
    std::vector<BlockPos>                    mOverrides;
    uint64_t                                 mRevision = UINT64_MAX, mViewRevision = UINT64_MAX, mNextScan = 0;
    size_t                                   mCursor    = 0;
    bool                                     mRequested = false, mLayerOnly = false, mActive = false, mEnabled = false;
};
} 
