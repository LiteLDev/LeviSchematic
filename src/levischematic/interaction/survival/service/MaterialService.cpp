#include "levischematic/interaction/survival/service/MaterialService.h"
#include "levischematic/interaction/survival/inventory/InventoryAccess.h"
#include "levischematic/schematic/io/SchematicLoader.h"
#include "levischematic/schematic/placement/model/PlacementGeometry.h"
#include "levischematic/schematic/placement/model/VolumeIndex.h"
#include "levischematic/shared/i18n/I18n.h"
#include "levischematic/shared/io/AtomicFile.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include "mc/world/level/levelgen/structure/StructureTemplateData.h"
#include <chrono>
#include <mutex>
#include <thread>

namespace levischematic::app {
struct MaterialFileJob {
    struct Read {
        std::mutex                                 mutex;
        bool                                       done = false;
        std::optional<placement::AssetPreparation> prepared;
        std::string                                error;
    };
    std::string                                      key;
    std::shared_ptr<const placement::SchematicAsset> asset;
    std::optional<placement::AssetPreparation>       prepared;
    std::vector<placement::PlacementInstance>        leaves;
    size_t                                           leaf = 0, cursor = 0;
    std::shared_ptr<Read>                            read = std::make_shared<Read>();
    std::jthread                                     worker;
};
namespace {
uint64_t nowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count()
    );
}
std::string utf8(std::filesystem::path const& path) {
    auto text = path.u8string();
    return {reinterpret_cast<char const*>(text.data()), text.size()};
}
std::string fileKey(std::filesystem::path const& path) {
    std::error_code ec;
    auto            time = std::filesystem::last_write_time(path, ec);
    return utf8(path) + "|" + (ec ? "?" : std::to_string(time.time_since_epoch().count()));
}
int liquid(Block const* block) {
    if (!block) return 0;
    auto const& name = block->getBlockType().mNameInfo->mFullName->getString();
    if (name == "minecraft:water" || name == "minecraft:flowing_water" || name == "minecraft:bubble_column") return 1;
    if (name == "minecraft:lava" || name == "minecraft:flowing_lava") return 2;
    return 0;
}
} // namespace
MaterialService::MaterialService(PlacementService& placements) : mPlacements(placements) {
    mScan.includeDisabled = true;
}
MaterialService::~MaterialService() = default;
void MaterialService::request(bool layerOnly) {
    mArea.reset();
    mFile.reset();
    mSource    = MaterialSource::Placement;
    mLayerOnly = layerOnly;
    mRequested = true;
}
void MaterialService::requestFile(std::filesystem::path const& path) {
    mArea.reset();
    mActive   = false;
    mSource   = MaterialSource::File;
    mFilePath = path;
    auto key  = fileKey(path);
    if (auto it = mFileCache.find(key); it != mFileCache.end()) {
        mFile.reset();
        mPublished         = it->second;
        mPublished.version = ++mVersion;
        mRefreshInventory  = true;
        return;
    }
    if (mFile && mFile->key == key) return;
    mFile      = std::make_unique<MaterialFileJob>();
    mFile->key = key;
    std::error_code ec;
    auto            normalized = std::filesystem::weakly_canonical(path, ec);
    for (auto const& record : mPlacements.resources().list()) {
        std::error_code other;
        if (!record.path.empty() && record.content && !ec
            && std::filesystem::weakly_canonical(record.path, other) == normalized && !other) {
            mFile->asset = record.content;
            break;
        }
    }
    if (!mFile->asset)
        mFile->worker = std::jthread([read = mFile->read, path](std::stop_token stop) {
            auto             result = placement::SchematicLoader::read(path, stop);
            std::scoped_lock lock(read->mutex);
            if (result) read->prepared.emplace(std::move(result).value());
            else read->error = result.error().describe(path.string());
            read->done = true;
        });
    beginWorking();
    mWorking.source    = MaterialSource::File;
    mWorking.file      = true;
    mWorking.fileName  = utf8(path.stem());
    mPublished         = mWorking;
    mPublished.version = ++mVersion;
}
void MaterialService::requestArea(std::vector<std::pair<BlockPos, BlockPos>> boxes, int dimension, std::string name) {
    mFile.reset();
    mSource = MaterialSource::Area;
    mArea.emplace();
    for (auto [a, b] : boxes)
        mArea->boxes.push_back({
            {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)},
            {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}
        });
    mArea->dimension = dimension;
    mArea->name      = std::move(name);
    mArea->running   = true;
    mActive          = false;
    beginWorking();
    mWorking.source   = MaterialSource::Area;
    mWorking.area     = true;
    mWorking.areaName = mArea->name;
    for (auto const& [a, b] : mArea->boxes)
        mWorking.total += static_cast<size_t>(b.x - a.x + 1) * (b.y - a.y + 1) * (b.z - a.z + 1);
    mPublished         = mWorking;
    mPublished.version = ++mVersion;
}
void MaterialService::beginWorking() {
    mWorking          = {};
    mWorking.building = true;
    mWorking.stale    = true;
    mRows.clear();
    mWarnings.clear();
    mUnits.clear();
}
void MaterialService::reset() {
    mSource = MaterialSource::None;
    mArea.reset();
    mFile.reset();
    mActive    = false;
    mRequested = false;
    mRevision = mViewRevision = UINT64_MAX;
    mScanId                   = 0;
    mPlacementCache.clear();
    mFileCache.clear();
    mRows.clear();
    mUnits.clear();
    mWarnings.clear();
    mWorking           = {};
    mPublished         = {};
    mPublished.version = ++mVersion;
}
void MaterialService::clearCache() {
    switch (mSource) {
    case MaterialSource::Area: {
        auto area = std::move(*mArea);
        requestArea(std::move(area.boxes), area.dimension, std::move(area.name));
        break;
    }
    case MaterialSource::File:
        mFileCache.clear();
        mFile.reset();
        requestFile(mFilePath);
        break;
    case MaterialSource::Placement:
        mPlacementCache.clear();
        mPublished         = {};
        mPublished.version = ++mVersion;
        mScanId            = 0;
        mRequested         = true;
        break;
    default:
        break;
    }
}
void MaterialService::publish(LocalPlayer& player) {
    auto inventory = survival::inventoryCounts(player);
    for (auto& [key, row] : mRows) {
        row.counts.available = inventory[key];
        mWorking.rows.push_back(row);
    }
    mWorking.warnings.assign(mWarnings.begin(), mWarnings.end());
    mWorking.building = false;
    mWorking.stale    = false;
    mWorking.version  = ++mVersion;
    mPublished        = std::move(mWorking);
    mWorking          = {};
}
void MaterialService::tickArea(LocalPlayer& player, util::WorkBudget const& budget) {
    auto& area = *mArea;
    if (!area.running || static_cast<int>(player.getDimensionId()) != area.dimension) return;
    auto& source = player.getDimension().getBlockSourceFromMainChunkSource();
    while (area.box < area.boxes.size() && budget.available()) {
        auto [a, b] = area.boxes[area.box];
        uint64_t sx = static_cast<uint64_t>(b.x - a.x) + 1, sz = static_cast<uint64_t>(b.z - a.z) + 1;
        uint64_t volume = sx * sz * (static_cast<uint64_t>(b.y - a.y) + 1);
        for (int step = 0; step < 256 && area.cursor < volume; ++step, ++area.cursor) {
            ++mWorking.scanned;
            BlockPos at{
                a.x + static_cast<int>(area.cursor % sx),
                a.y + static_cast<int>(area.cursor / (sx * sz)),
                a.z + static_cast<int>(area.cursor / sx % sz)
            };
            if (!source.hasChunksAt(at, 0, false)) {
                ++mWorking.unresolved;
                continue;
            }
            if (auto extra = survival::secondLayerItem(source.getExtraBlock(at))) {
                auto& row = mRows[extra->key];
                row.item  = extra->key;
                row.label = extra->label;
                ++row.counts.total;
            }
            auto const& block = source.getBlock(at);
            if (block.isAir()) continue;
            if (liquid(&block)) {
                if (auto item = survival::secondLayerItem(block)) {
                    auto& row = mRows[item->key];
                    row.item  = item->key;
                    row.label = item->label;
                    ++row.counts.total;
                }
                continue;
            }
            auto item = block.getBlockType().asItemInstance(block, nullptr);
            if (!item || !item.mItem) {
                ++mWorking.unresolved;
                continue;
            }
            ItemStack stack;
            stack.reinit(*item.mItem, 1, item.getAuxValue());
            if (!stack) continue;
            auto  key = survival::itemKey(stack);
            auto& row = mRows[key];
            row.item  = key;
            if (row.label.empty()) row.label = stack.getDescriptionName();
            ++row.counts.total;
        }
        if (area.cursor >= volume) {
            ++area.box;
            area.cursor = 0;
        }
    }
    mPublished.scanned = mWorking.scanned;
    if (area.box < area.boxes.size()) return;
    area.running = false;
    publish(player);
}
void MaterialService::add(survival::ItemRequirement const& requirement, verifier::VerificationStatus status) {
    using S      = verifier::VerificationStatus;
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
void MaterialService::accumulate(
    placement::PlacementInstance const& p,
    placement::ExpectedPosition const&  position,
    BlockSource*                        world
) {
    using S = verifier::VerificationStatus;
    if (!position.block) {
        ++mWorking.unresolved;
        return;
    }
    if (position.sourceIndex != SIZE_MAX && p.asset && p.asset->structure) {
        auto const& extra = p.asset->structure->mStructureTemplateData->mExtraBlockIndices.get();
        auto        index = placement::secondaryLayerIndex(
            extra ? &*extra : nullptr,
            position.sourceIndex,
            p.asset->editedCells.contains(position.sourceIndex)
        );
        Block const* layer = index && *index < p.asset->palette.size() ? p.asset->palette[*index] : nullptr;
        if (layer && !position.block->isAir())
            if (auto item = survival::secondLayerItem(*layer)) {
                auto status = S::MissingBlock;
                if (world) {
                    if (!world->hasChunksAt(position.world, 0, false)) status = S::Unknown;
                    else if (
                        liquid(&world->getExtraBlock(position.world)) == liquid(layer)
                        || liquid(&world->getBlock(position.world)) == liquid(layer)
                    )
                        status = S::Matched;
                }
                add(*item, status);
            }
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
    auto status = S::MissingBlock;
    if (world) {
        auto statusAt = [&](BlockPos const& at) {
            auto expected = placement::expectedAt(p, at);
            return survival::compareBlocks(
                expected ? expected->block : nullptr,
                world->hasChunksAt(at, 0, false) ? &world->getBlock(at) : nullptr
            );
        };
        status = statusAt(position.world);
        if (status == S::BlockMismatch && liquid(position.block)
            && liquid(position.block) == liquid(&world->getBlock(position.world)))
            status = S::Matched;
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
    }
    for (auto const& requirement : items.materials) add(requirement, status);
}
void MaterialService::tickPlacement(LocalPlayer& player, util::WorkBudget const& budget) {
    auto lock     = mPlacements.lockPlacements();
    auto selected = mPlacements.requireSelectedPlacement();
    if (!selected || selected.value()->dimensionId != static_cast<int>(player.getDimensionId())) {
        if (mPublished.placementId || !mPublished.stale || mPublished.source != MaterialSource::Placement) {
            mPublished         = {};
            mPublished.source  = MaterialSource::Placement;
            mPublished.version = ++mVersion;
        }
        mActive = false;
        mScanId = 0;
        return;
    }
    auto const& p   = *selected.value();
    auto        now = nowMs();
    if (mRequested || mScanId != p.id || mRevision != p.revision || mViewRevision != mPlacements.viewState().revision
        || mScanLayer != mLayerOnly || (!mActive && now >= mNextScan)) {
        auto current = [&](MaterialReport const& r) {
            return r.source == MaterialSource::Placement && r.placementId == p.id && !r.stale
                && r.layerOnly == mLayerOnly;
        };
        if (!current(mPublished))
            if (auto it = mPlacementCache.find(p.id); it != mPlacementCache.end() && current(it->second)) {
                mPublished         = it->second;
                mPublished.version = ++mVersion;
            }
        mSilent = current(mPublished);
        beginWorking();
        mWorking.source      = MaterialSource::Placement;
        mWorking.placementId = p.id;
        mWorking.layerOnly   = mLayerOnly;
        mPlacements.prepareScan(mScan, p.id);
        mWorking.total = mScan.size();
        mCursor        = 0;
        mScanId        = p.id;
        mScanLayer     = mLayerOnly;
        mRevision      = p.revision;
        mViewRevision  = mPlacements.viewState().revision;
        mRequested     = false;
        mActive        = true;
        if (!mSilent) {
            mPublished         = mWorking;
            mPublished.version = ++mVersion;
        }
    }
    auto& source = player.getDimension().getBlockSourceFromMainChunkSource();
    while (mActive && mCursor < mWorking.total && budget.available()) {
        auto [leaf, position] = mScan.at(mCursor++);
        ++mWorking.scanned;
        if (!leaf || !position || (mLayerOnly && !mPlacements.viewState().layerRange.containsPosition(position->world)))
            continue;
        accumulate(*leaf, *position, &source);
    }
    if (mActive && mCursor == mWorking.total) {
        mActive   = false;
        mNextScan = now + 1000;
        auto id   = mWorking.placementId;
        publish(player);
        mPlacementCache[id] = mPublished;
    } else if (mActive && !mSilent) {
        mPublished.scanned = mWorking.scanned;
        mPublished.total   = mWorking.total;
    }
}
void MaterialService::tickFile(LocalPlayer& player, util::WorkBudget const& budget) {
    auto& f    = *mFile;
    auto  fail = [&](std::string error) {
        mWarnings.insert(std::move(error));
        mRows.clear();
        publish(player);
        mFile.reset();
    };
    if (!f.asset) {
        if (!f.prepared) {
            std::scoped_lock lock(f.read->mutex);
            if (!f.read->done) return;
            if (!f.read->error.empty()) {
                auto error = f.read->error;
                return fail(error);
            }
            f.prepared = std::move(f.read->prepared);
            if (!f.prepared) return fail(tr("msg.survival.schematic_read_failed"));
        }
        try {
            if (!placement::SchematicLoader::advance(*f.prepared, player.getLevel(), budget)) return;
        } catch (std::exception const& e) {
            return fail(e.what());
        }
        f.asset = f.prepared->asset;
        f.prepared.reset();
    }
    if (f.leaves.empty()) {
        auto leaf = [&](std::shared_ptr<const placement::SchematicAsset> asset, BlockPos origin) {
            if (!asset) return;
            placement::PlacementInstance p;
            p.asset         = std::move(asset);
            p.origin        = origin;
            mWorking.total += p.asset->indices.size();
            f.leaves.push_back(std::move(p));
        };
        if (f.asset->regions.empty()) leaf(f.asset, {0, 0, 0});
        for (auto const& region : f.asset->regions) leaf(region.content, region.offset);
        mPublished.total = mWorking.total;
        if (f.leaves.empty()) return fail(tr("msg.survival.empty_schematic"));
    }
    while (f.leaf < f.leaves.size() && budget.available()) {
        auto const& p = f.leaves[f.leaf];
        auto        n = p.asset->indices.size();
        for (int step = 0; step < 256 && f.cursor < n; ++step, ++f.cursor) {
            ++mWorking.scanned;
            if (auto position = placement::expectedPosition(p, f.cursor)) accumulate(p, *position, nullptr);
        }
        if (f.cursor >= n) {
            ++f.leaf;
            f.cursor = 0;
        }
    }
    mPublished.scanned = mWorking.scanned;
    if (f.leaf < f.leaves.size()) return;
    auto key = f.key;
    publish(player);
    mFileCache[key] = mPublished;
    mFile.reset();
}
void MaterialService::tick(LocalPlayer& player, util::WorkBudget const& budget) {
    switch (mSource) {
    case MaterialSource::Area:
        if (mArea) tickArea(player, budget);
        break;
    case MaterialSource::File:
        if (mFile) tickFile(player, budget);
        break;
    case MaterialSource::Placement:
        tickPlacement(player, budget);
        break;
    default:
        return;
    }
    if (mRefreshInventory && !mPublished.building && !mPublished.stale) {
        auto inventory = survival::inventoryCounts(player);
        for (auto& row : mPublished.rows) row.counts.available = inventory[row.item];
        mPublished.version = ++mVersion;
        mRefreshInventory  = false;
    }
}
std::string
MaterialService::exportReport(std::filesystem::path const& directory, ExportFormat format, int multiplier) const {
    if (mPublished.stale || mPublished.building) return tr("msg.survival.wait_for_current_analysis");
    uint64_t    m = static_cast<uint64_t>(std::max(1, multiplier));
    std::string data;
    auto        quote = [](std::string_view text) {
        std::string out = "\"";
        for (char c : text) {
            if (static_cast<unsigned char>(c) < 0x20) continue;
            if (c == '"' || c == '\\') out += '\\';
            out += c;
        }
        return out + '"';
    };
    auto rows = mPublished.rows;
    std::erase_if(rows, [&](auto const& r) { return mIgnored.contains(r.item); });
    if (format == ExportFormat::Csv) {
        data = "\xEF\xBB\xBFitem,aux,label,total,missing,wrong_block,wrong_state,unknown,available,to_gather\r\n";
        for (auto const& row : rows) {
            auto const& c  = row.counts;
            data          += survival::csvCell(row.item.name) + "," + std::to_string(row.item.aux) + ","
                           + survival::csvCell(row.label);
            for (auto n :
                 {c.total * m,
                  c.missing * m,
                  c.wrongBlock * m,
                  c.wrongState * m,
                  c.unknown * m,
                  c.available,
                  c.toGather()})
                data += "," + std::to_string(n);
            data += "\r\n";
        }
    } else if (format == ExportFormat::Json) {
        data = "[\n";
        for (size_t i = 0; i < rows.size(); ++i) {
            auto const& c = rows[i].counts;
            data += "  {\"item\": " + quote(rows[i].item.name) + ", \"aux\": " + std::to_string(rows[i].item.aux)
                  + ", \"label\": " + quote(rows[i].label) + ", \"total\": " + std::to_string(c.total * m)
                  + ", \"missing\": " + std::to_string(c.remaining() * m)
                  + ", \"available\": " + std::to_string(c.available) + "}" + (i + 1 < rows.size() ? ",\n" : "\n");
        }
        data += "]\n";
    } else {
        size_t width = 4;
        for (auto const& row : rows) width = std::max(width, row.label.size());
        auto pad = [](std::string text, size_t n) {
            if (text.size() < n) text.append(n - text.size(), ' ');
            return text;
        };
        data = "\xEF\xBB\xBF" + pad("Item", width) + " | Total      | Missing    | Available\r\n";
        for (auto const& row : rows) {
            auto const& c  = row.counts;
            data          += pad(row.label, width) + " | " + pad(std::to_string(c.total * m), 10) + " | "
                           + pad(std::to_string(c.remaining() * m), 10) + " | " + std::to_string(c.available) + "\r\n";
        }
    }
    auto stem = mPublished.area ? "area-" + std::to_string(nowMs())
              : mPublished.file ? "schematic-" + std::to_string(nowMs())
                                : "placement-" + std::to_string(mPublished.placementId) + "-" + std::to_string(nowMs());
    auto path = directory / "materials"
              / (stem
                 + (format == ExportFormat::Csv    ? ".csv"
                    : format == ExportFormat::Json ? ".json"
                                                   : ".txt"));
    try {
        util::writeAtomic(path, data, false);
        return tr("msg.survival.exported") + path.string();
    } catch (std::exception const& e) {
        return e.what();
    }
}
} // namespace levischematic::app
