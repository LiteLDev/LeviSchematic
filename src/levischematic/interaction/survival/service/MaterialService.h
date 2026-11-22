#pragma once
#include "levischematic/interaction/survival/inventory/BlockItems.h"
#include "levischematic/schematic/placement/query/ProjectionScan.h"
#include "levischematic/schematic/placement/service/PlacementService.h"
#include "levischematic/shared/scheduling/WorkBudget.h"
#include <map>
#include <memory>
#include <optional>
#include <set>
class LocalPlayer;
class BlockSource;
namespace levischematic::app {
struct MaterialRow {
    survival::ItemKey        item;
    std::string              label;
    survival::MaterialCounts counts;
};
enum class MaterialSource { None, Placement, File, Area };
struct MaterialReport {
    MaterialSource           source      = MaterialSource::None;
    uint32_t                 placementId = 0;
    bool                     area = false, file = false;
    std::string              areaName, fileName;
    bool                     building = false, stale = true, layerOnly = false;
    size_t                   scanned = 0, total = 0, unresolved = 0;
    uint64_t                 version = 0;
    std::vector<MaterialRow> rows;
    std::vector<std::string> warnings;
};
struct MaterialArea {
    std::vector<std::pair<BlockPos, BlockPos>> boxes;
    int                                        dimension = 0;
    std::string                                name;
    size_t                                     box     = 0;
    uint64_t                                   cursor  = 0;
    bool                                       running = false;
};
struct MaterialFileJob;
class MaterialService {
public:
    explicit MaterialService(PlacementService& placements);
    ~MaterialService();
    void           request(bool layerOnly);
    void           requestFile(std::filesystem::path const& path);
    void           requestArea(std::vector<std::pair<BlockPos, BlockPos>> boxes, int dimension, std::string name);
    MaterialSource source() const { return mSource; }
    void           reset();
    void           refreshInventory() { mRefreshInventory = true; }
    void           tick(LocalPlayer&, util::WorkBudget const&);
    MaterialReport report() const { return mPublished; }
    enum class ExportFormat { Text, Csv, Json };
    std::string exportCsv(std::filesystem::path const& directory) const {
        return exportReport(directory, ExportFormat::Csv, 1);
    }
    std::string exportReport(std::filesystem::path const& directory, ExportFormat format, int multiplier) const;
    void        ignore(survival::ItemKey const& key) { mIgnored.insert(key); }
    void        clearIgnored() { mIgnored.clear(); }
    bool        ignored(survival::ItemKey const& key) const { return mIgnored.contains(key); }
    void        clearCache();

private:
    void accumulate(placement::PlacementInstance const&, placement::ExpectedPosition const&, BlockSource* world);
    void add(survival::ItemRequirement const&, verifier::VerificationStatus);
    void publish(LocalPlayer&);
    void tickPlacement(LocalPlayer&, util::WorkBudget const&);
    void tickFile(LocalPlayer&, util::WorkBudget const&);
    void tickArea(LocalPlayer&, util::WorkBudget const&);
    void beginWorking();
    PlacementService&                        mPlacements;
    MaterialSource                           mSource = MaterialSource::None;
    placement::ProjectionScan                mScan;
    MaterialReport                           mPublished, mWorking;
    std::map<survival::ItemKey, MaterialRow> mRows;
    std::set<std::string>                    mWarnings;
    std::set<uint64_t>                       mUnits;
    uint64_t mRevision = UINT64_MAX, mViewRevision = UINT64_MAX, mNextScan = 0, mVersion = 0;
    uint32_t mScanId    = 0;
    size_t   mCursor    = 0;
    bool     mRequested = false, mLayerOnly = false, mActive = false, mSilent = false, mRefreshInventory = false,
             mScanLayer = false;
    std::set<survival::ItemKey>           mIgnored;
    std::optional<MaterialArea>           mArea;
    std::unique_ptr<MaterialFileJob>      mFile;
    std::filesystem::path                 mFilePath;
    std::map<uint32_t, MaterialReport>    mPlacementCache;
    std::map<std::string, MaterialReport> mFileCache;
};
} // namespace levischematic::app
