#include "ResourceService.h"
#include "levischematic/schematic/placement/PlacementGeometry.h"
#include "levischematic/schematic/placement/SchematicLoader.h"
#include "levischematic/schematic/placement/SchematicSetIO.h"
#include "levischematic/schematic/placement/VolumeIndex.h"
#include "levischematic/util/AtomicFile.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/nbt/CompoundTag.h"
#include <algorithm>
#include <fstream>
#include <future>

namespace levischematic::app {
using namespace placement;
struct ResourceService::Edit {
    uint64_t                        resource;
    PlacementInstance               placement;
    std::shared_ptr<SchematicAsset> content;
    editor::LayerRange              range;
    EditRequest                     request;
    Block const*                    target      = nullptr;
    int                             replacement = -1;
    size_t                          cursor = 0, rebuild = 0;
    size_t                          scanTotal = 0;
    BlockPos                        local;
    BlockPos                        localStep{0, 0, 0};
};
struct ResourceService::Reload {
    uint64_t                                                    resource;
    std::shared_ptr<const SchematicAsset>                       previous;
    std::stop_source                                            stop;
    std::future<Expected<AssetPreparation, LoadPlacementError>> reader;
    std::optional<AssetPreparation>                             pending;
};
ResourceService::ResourceService(placement::PlacementStore& store, placement::ResourceState& state)
: mStore(store),
  mResources(state.records),
  mNext(state.nextId) {}
ResourceService::~ResourceService() {
    if (mReload) mReload->stop.request_stop();
}
std::shared_ptr<const SchematicAsset>
ResourceService::adopt(std::shared_ptr<const SchematicAsset> asset, std::filesystem::path const& path) {
    auto normalized = std::filesystem::weakly_canonical(path);
    for (auto& [id, resource] : mResources)
        if (resource.path == normalized) {
            if (asset->modified && !asset->resourceId && asset != resource.content) {
                auto restored        = std::make_shared<SchematicAsset>(*asset);
                restored->resourceId = id;
                publish(resource, restored);
            }
            return resource.content;
        }
    auto value        = std::make_shared<SchematicAsset>(*asset);
    value->resourceId = mNext++;
    mResources.emplace(value->resourceId, ResourceRecord{value->resourceId, normalized, value});
    return value;
}
std::vector<ResourceRecord> ResourceService::list() {
    
    
    for (auto id : mStore.state().order) {
        auto* p = mStore.get(id);
        if (!p || !p->asset) continue;
        if (!p->asset->resourceId || !mResources.contains(p->asset->resourceId)) {
            auto previous = p->asset;
            auto content  = adopt(previous, p->filePath);
            mStore.replaceAsset(previous, content);
        }
    }
    std::vector<ResourceRecord> result;
    for (auto const& [id, record] : mResources) result.push_back(record);
    return result;
}
void ResourceService::publish(ResourceRecord& resource, std::shared_ptr<const SchematicAsset> next) {
    auto previous = resource.content;
    mStore.replaceAsset(previous, next);
    resource.content = std::move(next);
    mStore.markResourceChange();
}
Result<uint64_t, std::string> ResourceService::createPlacement(uint64_t resource, BlockPos origin, int dimension) {
    try {
        auto it = mResources.find(resource);
        if (it == mResources.end()) return Result<uint64_t, std::string>::failure("Resource not found.");
        auto const& r = it->second;
        return Result<uint64_t, std::string>::success(
            mStore.createPlacement(r.content, origin, dimension, r.content->defaultName, r.path)
        );
    } catch (std::exception const& e) {
        return Result<uint64_t, std::string>::failure(e.what());
    }
}
StatusResult<std::string> ResourceService::unload(uint64_t id, bool discard, bool removePlacements) {
    auto it = mResources.find(id);
    if (it == mResources.end()) return StatusResult<std::string>::failure("Resource not found.");
    if (mProgress.running) return StatusResult<std::string>::failure("Finish or cancel content operation first.");
    if (it->second.content->modified && !discard)
        return StatusResult<std::string>::failure("Unsaved edits; save or discard first.");
    std::vector<PlacementId> references;
    for (auto p : mStore.state().order)
        if (mStore.get(p)->asset == it->second.content) references.push_back(p);
    if (!references.empty() && !removePlacements)
        return StatusResult<std::string>::failure("Resource has placements; confirm removal.");
    for (auto p : references) (void)mStore.remove(p);
    mResources.erase(it);
    mStore.markResourceChange();
    return ok<std::string>();
}
StatusResult<std::string> ResourceService::save(uint64_t id, std::filesystem::path path, bool overwrite) {
    try {
        auto it = mResources.find(id);
        if (it == mResources.end()) throw std::runtime_error("Resource not found.");
        if (mProgress.running) throw std::runtime_error("Finish or cancel content operation first.");
        if (path.empty()) path = it->second.path;
        if (resourceSourceChanged(*it->second.content))
            throw std::runtime_error("Source changed since loading; use Save As.");
        saveResource(*it->second.content, path, overwrite);
        auto next = savedSnapshot(*it->second.content, path);
        publish(it->second, next);
        return ok<std::string>();
    } catch (std::exception const& e) {
        return StatusResult<std::string>::failure(e.what());
    }
}
StatusResult<std::string> ResourceService::reload(uint64_t id, bool discard) {
    try {
        auto it = mResources.find(id);
        if (it == mResources.end()) throw std::runtime_error("Resource not found.");
        if (mProgress.running || mReload) throw std::runtime_error("Finish or cancel content operation first.");
        if (it->second.content->modified && !discard) throw std::runtime_error("Unsaved edits; save or discard first.");
        auto job      = std::make_unique<Reload>();
        job->resource = id;
        job->previous = it->second.content;
        job->reader   = std::async(std::launch::async, [path = it->second.path, token = job->stop.get_token()] {
            return SchematicLoader::read(path, token);
        });
        mReload       = std::move(job);
        mProgress     = {true, 0, 0, 0, "Reload queued"};
        return ok<std::string>();
    } catch (std::exception const& e) {
        return StatusResult<std::string>::failure(e.what());
    }
}
StatusResult<std::string> ResourceService::saveAs(PlacementId id, std::filesystem::path path, bool overwrite) {
    try {
        auto* p = mStore.get(id);
        if (!p || p->parentId || !p->asset) throw std::runtime_error("Select a managed placement first.");
        if (mProgress.running) throw std::runtime_error("Finish or cancel editing first.");
        auto target = std::filesystem::weakly_canonical(path);
        for (auto const& [id, r] : mResources)
            if (r.path == target) throw std::runtime_error("Target is already loaded; choose another path.");
        auto next            = std::make_shared<SchematicAsset>(*p->asset);
        next->resourceId     = mNext++;
        next->defaultName    = path.stem().string();
        next->sourceConflict = false;
        next->modified       = false;
        saveResource(*next, path, overwrite);
        auto saved = savedSnapshot(*next, target);
        mResources.emplace(saved->resourceId, ResourceRecord{saved->resourceId, target, saved});
        (void)mStore.bindAsset(id, saved, target);
        return ok<std::string>();
    } catch (std::exception const& e) {
        return StatusResult<std::string>::failure(e.what());
    }
}
StatusResult<std::string> ResourceService::edit(EditRequest const& request, editor::LayerRange const& range) {
    if (mProgress.running) return StatusResult<std::string>::failure("Content operation already running.");
    (void)list();
    auto* p = mStore.get(request.placement);
    if (!p || !p->asset) return StatusResult<std::string>::failure("Placement not found.");
    auto local = toLocal(*p, request.world);
    if (!local) return StatusResult<std::string>::failure("Coordinate overflow.");
    auto size = p->asset->size;
    if (local->x < 0 || local->y < 0 || local->z < 0 || local->x >= size.x || local->y >= size.y || local->z >= size.z)
        return StatusResult<std::string>::failure("Edit lies outside the resource.");
    auto index = volumeIndex({local->x, local->y, local->z}, {size.x, size.y, size.z});
    if (p->asset->indices[index] < 0) return StatusResult<std::string>::failure("Ignored positions are not editable.");
    auto value = std::make_unique<Edit>();
    auto root  = p->parentId ? mStore.get(p->parentId) : p;
    if (!root) return StatusResult<std::string>::failure("Placement was removed.");
    value->resource  = root->asset->resourceId;
    value->placement = *p;
    value->content   = std::make_shared<SchematicAsset>(*p->asset);
    value->content->localBlocks.clear();
    value->request = request;
    value->local   = *local;
    value->range   = range;
    if (!range.containsPosition(request.world))
        return StatusResult<std::string>::failure("Target is outside the render range.");
    value->scanTotal = request.match == EditMatch::Single ? 1 : value->content->indices.size();
    if (request.match == EditMatch::Line) {
        int64_t magnitude = std::abs(int64_t(request.direction.x)) + std::abs(int64_t(request.direction.y))
                          + std::abs(int64_t(request.direction.z));
        auto    next      = magnitude == 1 ? checkedPosition(
                                                 {int64_t(request.world.x) + request.direction.x,
                                                  int64_t(request.world.y) + request.direction.y,
                                                  int64_t(request.world.z) + request.direction.z}
                                             )
                                           : std::nullopt;
        auto    localNext = next ? toLocal(*p, *next) : std::nullopt;
        if (!localNext) return StatusResult<std::string>::failure("Invalid line direction or coordinate overflow.");
        value->localStep = {localNext->x - local->x, localNext->y - local->y, localNext->z - local->z};
        int delta[3]{value->localStep.x, value->localStep.y, value->localStep.z};
        int at[3]{local->x, local->y, local->z}, extent[3]{size.x, size.y, size.z};
        for (int axis = 0; axis < 3; ++axis)
            if (delta[axis])
                value->scanTotal = static_cast<size_t>(delta[axis] > 0 ? extent[axis] - at[axis] : at[axis] + 1);
    }
    value->target = p->asset->palette[p->asset->indices[index]];
    auto block    = localBlock(*p, request.replacement);
    if (!block) {
        auto air = Block::tryGetFromRegistry(HashedString{"minecraft:air"});
        if (!air) return StatusResult<std::string>::failure("Air unavailable.");
        block = &*air;
    }
    auto& palette      = value->content->palette;
    auto  found        = std::find(palette.begin(), palette.end(), block);
    value->replacement = static_cast<int>(found - palette.begin());
    if (found == palette.end()) palette.push_back(block);
    mProgress = {true, 0, value->scanTotal, 0, "Editing; changes publish together"};
    mEdit     = std::move(value);
    return ok<std::string>();
}
void ResourceService::cancel() {
    mEdit.reset();
    if (mReload) mReload->stop.request_stop();
    mProgress.running = false;
    mProgress.detail  = "Cancelled; content unchanged";
}
void ResourceService::tick(util::WorkBudget const& budget) {
    if (mReload) {
        auto& job = *mReload;
        try {
            if (!job.pending) {
                if (job.reader.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
                auto pending = job.reader.get();
                if (job.stop.stop_requested()) {
                    mReload.reset();
                    return;
                }
                if (!pending) throw std::runtime_error(pending.error().describe("resource reload"));
                job.pending = std::move(pending).value();
            }
            if (job.stop.stop_requested()) {
                mReload.reset();
                return;
            }
            if (!SchematicLoader::advance(*job.pending, budget)) return;
            auto it = mResources.find(job.resource);
            if (it == mResources.end() || it->second.content != job.previous)
                throw std::runtime_error("Resource changed while reloading.");
            job.pending->asset->resourceId = job.resource;
            job.pending->asset->revision   = job.previous->revision + 1;
            publish(it->second, job.pending->asset);
            mProgress.running = false;
            mProgress.detail  = "Reload complete";
            mReload.reset();
        } catch (std::exception const& e) {
            mProgress.running = false;
            mProgress.detail  = e.what();
            mReload.reset();
        }
    }
    if (!mEdit) return;
    auto& e       = *mEdit;
    auto& content = *e.content;
    auto  size    = content.size;
    while (e.cursor < e.scanTotal && budget.available()) {
        auto step = e.cursor++;
        auto i    = step;
        if (e.request.match == EditMatch::Single || e.request.match == EditMatch::Line)
            i = volumeIndex(
                {e.local.x + static_cast<int>(step) * e.localStep.x,
                 e.local.y + static_cast<int>(step) * e.localStep.y,
                 e.local.z + static_cast<int>(step) * e.localStep.z},
                {size.x, size.y, size.z}
            );
        auto     pos = volumePosition(i, {size.x, size.y, size.z});
        BlockPos at{pos[0], pos[1], pos[2]};
        auto     world = toWorld(e.placement, at);
        auto     old   = content.indices[i];
        if (!world || old < 0 || !e.range.containsPosition(*world)) {
            if (e.request.match == EditMatch::Line) e.cursor = e.scanTotal;
            continue;
        }
        auto block = content.palette[old];
        bool match = at == e.local;
        if (e.request.match == EditMatch::State) match = block == e.target;
        if (e.request.match == EditMatch::Type)
            match = block && e.target && &block->getBlockType() == &e.target->getBlockType();
        if (e.request.match == EditMatch::Line) {
            match = block == e.target;
            if (!match) e.cursor = e.scanTotal;
        }
        if (match && old != e.replacement) {
            content.indices[i] = e.replacement;
            content.editedCells.insert(i);
            ++mProgress.changed;
        }
    }
    mProgress.scanned = e.cursor;
    while (e.cursor == e.scanTotal && e.rebuild < content.indices.size() && budget.available()) {
        auto i     = e.rebuild++;
        int  index = content.indices[i];
        auto block = index >= 0 ? content.palette[index] : nullptr;
        if (!block || block->isAir()) continue;
        auto pos = volumePosition(i, {size.x, size.y, size.z});
        content.localBlocks.push_back({
            {pos[0], pos[1], pos[2]},
            block
        });
    }
    if (e.rebuild == content.indices.size()) {
        content.modified = content.modified || mProgress.changed != 0;
        ++content.revision;
        auto it = mResources.find(e.resource);
        if (it != mResources.end()) {
            auto current = it->second.content;
            if (e.placement.regionId.empty()) {
                if (current == e.placement.asset) publish(it->second, e.content);
            } else {
                auto next = std::make_shared<SchematicAsset>(*current);
                for (auto& region : next->regions)
                    if (region.id == e.placement.regionId && region.content == e.placement.asset) {
                        region.content = e.content;
                        next->modified = true;
                        ++next->revision;
                        publish(it->second, next);
                        break;
                    }
            }
        }
        mProgress.running = false;
        mProgress.detail  = "Edit complete";
        mEdit.reset();
    }
}
} 
