#include "MaterialService.h"
#include "levischematic/schematic/placement/PlacementGeometry.h"
#include "levischematic/survival/InventoryAccess.h"
#include "levischematic/util/AtomicFile.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include "mc/world/level/levelgen/structure/StructureTemplateData.h"
#include <chrono>

namespace levischematic::app {
namespace {
uint64_t nowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count()
    );
}
} 
void MaterialService::request(bool layerOnly) {
    mLayerOnly = layerOnly;
    mRequested = true;
    mEnabled   = true;
}
void MaterialService::invalidate() {
    mPublished.stale = true;
    mActive          = false;
    mRequested       = mEnabled;
    mUnits.clear();
    mRows.clear();
    mOverrides.clear();
}
void MaterialService::accumulate(
    placement::PlacementInstance const& p,
    placement::ExpectedPosition const&  position,
    LocalPlayer&                        player
) {
    if (!position.block) {
        ++mWorking.unresolved;
        return;
    }
    if (position.block->isAir()) return;
    auto items = survival::resolveBlockItems(p, position);
    if (!items.unsupported.empty()) {
        ++mWorking.unresolved;
        mWarnings.insert(items.unsupported);
    }
    BlockPos canonical = position.world;
    auto     partner   = selection::tool::offset({canonical.x, canonical.y, canonical.z}, items.rule.partnerOffset);
    if (items.rule.secondary && partner) canonical = {(*partner)[0], (*partner)[1], (*partner)[2]};
    
    if (!mUnits.insert(util::encodePosKey(canonical)).second) return;
    auto& source   = player.getDimension().getBlockSourceFromMainChunkSource();
    auto  statusAt = [&](BlockPos const& at) {
        auto expected = placement::expectedAt(p, at);
        return survival::compareBlocks(
            expected ? expected->block : nullptr,
            source.hasChunksAt(at, 0, false) ? &source.getBlock(at) : nullptr
        );
    };
    auto status = statusAt(position.world);
    using S     = verifier::VerificationStatus;
    if (items.rule.partnerOffset != survival::Position{} && partner) {
        BlockPos other{(*partner)[0], (*partner)[1], (*partner)[2]};
        auto     secondary = statusAt(other);
        auto     priority  = [](S value) {
            switch (value) {
            case S::Unknown:
                return 5;
            case S::MissingBlock:
                return 4;
            case S::BlockMismatch:
                return 3;
            case S::PropertyMismatch:
                return 2;
            default:
                return 0;
            }
        };
        if (priority(secondary) > priority(status)) status = secondary;
    }
    for (auto const& requirement : items.materials) {
        auto& row    = mRows[requirement.key];
        row.item     = requirement.key;
        row.label    = requirement.label;
        auto  count  = requirement.count;
        auto& c      = row.counts;
        c.total     += count;
        switch (status) {
        case S::Unknown:
            c.unknown += count;
            break;
        case S::MissingBlock:
            c.missing += count;
            break;
        case S::BlockMismatch:
            c.wrongBlock += count;
            break;
        case S::PropertyMismatch:
            c.wrongState += count;
            break;
        default:
            break;
        }
    }
}
void MaterialService::tick(LocalPlayer& player, util::WorkBudget const& budget) {
    if (!mEnabled) return;
    auto lock     = mPlacements.lockPlacements();
    auto selected = mPlacements.requireSelectedPlacement();
    if (!selected || selected.value()->dimensionId != static_cast<int>(player.getDimensionId())) {
        mPublished       = {};
        mPublished.stale = true;
        mActive          = false;
        return;
    }
    auto const& p   = *selected.value();
    auto        now = nowMs();
    if (mRequested || mPublished.placementId != p.id || mRevision != p.revision
        || mViewRevision != mPlacements.viewState().revision || (!mActive && now >= mNextScan)) {
        mWorking             = {};
        mWorking.placementId = p.id;
        mWorking.layerOnly   = mLayerOnly;
        mWorking.building    = true;
        mWorking.stale       = true;
        mPlacements.prepareScan(mScan,p.id);
        mWorking.total = mScan.size();
        if (p.asset && p.asset->structure)
            mWorking.secondLayer = p.asset->structure->mStructureTemplateData->mExtraBlockIndices->has_value();
        mCursor = 0;
        mUnits.clear();
        mRows.clear();
        mWarnings.clear();
        mOverrides.clear();
        for (auto const& [key, value] : p.overrides) {
            (void)value;
            if (auto world = placement::toWorld(p, util::decodePosKey(key))) mOverrides.push_back(*world);
        }
        mRevision              = p.revision;
        mViewRevision          = mPlacements.viewState().revision;
        mRequested             = false;
        mActive                = true;
        mPublished.building    = true;
        mPublished.stale       = true;
        mPublished.placementId = p.id;
    }
    while (mActive && mCursor < mWorking.total && budget.available()) {
        auto [leaf, position] = mScan.at(mCursor++);
        ++mWorking.scanned;
        if (!leaf || !position || (mLayerOnly && !mPlacements.viewState().layerRange.containsPosition(position->world))) continue;
        accumulate(*leaf, *position, player);
    }
    if (mActive && mCursor == mWorking.total) {
        auto inventory = survival::inventoryCounts(player);
        for (auto& [key, row] : mRows) {
            row.counts.available = inventory[key];
            mWorking.rows.push_back(row);
        }
        mWorking.warnings.assign(mWarnings.begin(), mWarnings.end());
        mWorking.building = false;
        mWorking.stale    = false;
        mPublished        = std::move(mWorking);
        mActive           = false;
        mNextScan         = now + 1000;
    } else if (mActive) {
        mPublished.scanned = mWorking.scanned;
        mPublished.total   = mWorking.total;
    }
}
std::string MaterialService::exportCsv(std::filesystem::path const& directory) const {
    if (mPublished.stale || mPublished.building) return "Wait for current analysis / 请等待统计完成";
    std::string data =
        "\xEF\xBB\xBFitem,aux,label,total,missing,wrong_block,wrong_state,unknown,available,to_gather\r\n";
    for (auto const& row : mPublished.rows) {
        auto const& c = row.counts;
        data +=
            survival::csvCell(row.item.name) + "," + std::to_string(row.item.aux) + "," + survival::csvCell(row.label);
        for (auto n : {c.total, c.missing, c.wrongBlock, c.wrongState, c.unknown, c.available, c.toGather()})
            data += "," + std::to_string(n);
        data += "\r\n";
    }
    auto path = directory / "materials"
              / ("placement-" + std::to_string(mPublished.placementId) + "-" + std::to_string(nowMs()) + ".csv");
    try {
        util::writeAtomic(path, data, false);
        return "Exported / 已导出: " + path.string();
    } catch (std::exception const& e) {
        return e.what();
    }
}
} 
