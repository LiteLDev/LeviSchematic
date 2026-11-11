#include "VerifierService.h"
#include "levischematic/schematic/placement/PlacementGeometry.h"
#include <algorithm>
#include <map>

namespace levischematic::verifier {
bool VerifierService::startTask(uint32_t id, bool layerOnly) {
    auto found = mPlacements.placements.find(id);
    if (found == mPlacements.placements.end() || !found->second.asset || !found->second.enabled) return false;
    VerificationTask task;
    if (auto previous = mTasks.find(id); previous != mTasks.end()) {
        task.ignored     = previous->second.ignored;
        task.highlighted = previous->second.highlighted;
    }
    task.scan.reset(mPlacements,id);
    task.placementId       = id;
    task.placementRevision = found->second.revision;
    auto const& p          = found->second;
    task.contentRevision   = p.contentRevision;
    task.origin            = p.origin;
    task.dimension         = p.dimensionId;
    task.asset             = p.asset.get();
    task.rotation          = p.rotation;
    task.mirror            = p.mirror;
    task.enabled           = p.enabled;
    task.viewRevision      = mView.revision;
    task.layerOnly         = layerOnly;
    mTasks.insert_or_assign(id, std::move(task));
    mHighlightTask = id;
    rebuildOverlay();
    return true;
}
void VerifierService::stopTask(uint32_t id) {
    if (auto it = mTasks.find(id); it != mTasks.end()) it->second.running = false;
}
void VerifierService::resetTask(uint32_t id) {
    mTasks.erase(id);
    if (mHighlightTask == id) {
        mHighlightTask = 0;
        rebuildOverlay();
    }
}
void VerifierService::ignoreGroup(uint32_t id, std::string const& group) {
    if (auto it = mTasks.find(id); it != mTasks.end()) {
        it->second.ignored.insert(group);
        rebuildOverlay();
    }
}
void VerifierService::clearIgnored(uint32_t id) {
    if (auto it = mTasks.find(id); it != mTasks.end()) {
        it->second.ignored.clear();
        rebuildOverlay();
    }
}
void VerifierService::toggleHighlight(uint32_t id, VerificationStatus status) {
    if (auto it = mTasks.find(id); it != mTasks.end()) {
        auto index = static_cast<size_t>(status);
        if (index >= it->second.highlighted.size()) return;
        it->second.highlighted[index] = !it->second.highlighted[index];
        mHighlightTask                = id;
        rebuildOverlay();
    }
}
void VerifierService::publishOverlay(VerificationTask const& task, BlockPos const& pos, VerificationCell const& cell) {
    if (task.placementId != mHighlightTask || !mState.taskOverlay) return;
    auto p = mPlacements.placements.find(task.placementId);
    if (p == mPlacements.placements.end()) return;
    auto& overlay  = *mState.taskOverlay;
    auto& chunk    = overlay.chunks[{p->second.dimensionId, util::subChunkKeyFromWorldPos(pos.x, pos.y, pos.z)}];
    auto  key      = util::encodePosKey(pos);
    auto  category = static_cast<size_t>(cell.status);
    bool  error    = category >= 2 && category < task.highlighted.size();
    if (error && task.highlighted[category] && !task.ignored.contains(verificationGroupId(cell)))
        chunk.cells.insert_or_assign(key, cell);
    else chunk.cells.erase(key);
    chunk.revision = ++overlay.revision;
}
void VerifierService::rebuildOverlay() {
    auto overlay   = std::make_shared<VerifierState>();
    overlay->epoch = ++mOverlayEpoch;
    if (mState.taskOverlay) mRetiredOverlays.push_back(std::move(mState.taskOverlay));
    mState.taskOverlay = std::move(overlay);
    mOverlayCursor     = 0;
    mOverlayRebuild    = true;
}
void VerifierService::tickTasks(util::WorkBudget const& budget) {
    size_t releaseCount = 0;
    while (!mRetiredOverlays.empty() && budget.available() && releaseCount++ < 512) {
        auto& chunks = mRetiredOverlays.front()->chunks;
        if (chunks.empty()) mRetiredOverlays.pop_front();
        else if (chunks.begin()->second.cells.empty()) chunks.erase(chunks.begin());
        else chunks.begin()->second.cells.erase(chunks.begin()->second.cells.begin());
    }
    if (mOverlayRebuild) {
        auto found = mTasks.find(mHighlightTask);
        if (found == mTasks.end() || found->second.stale) mOverlayRebuild = false;
        else {
            auto const& task  = found->second;
            size_t      count = 0;
            while (mOverlayCursor < task.keys.size() && budget.available() && count++ < 256) {
                auto key = task.keys[mOverlayCursor++];
                publishOverlay(task, util::decodePosKey(key), task.cells.at(key));
            }
            if (mOverlayCursor == task.keys.size()) mOverlayRebuild = false;
        }
    }
    std::vector<uint32_t> ids;
    ids.reserve(mTasks.size());
    for (auto const& [id, task] : mTasks) ids.push_back(id);
    if (ids.empty()) return;
    auto start = mTaskTurn++ % ids.size();
    for (size_t n = 0; n < ids.size() && budget.available(); ++n) {
        auto id        = ids[(start + n) % ids.size()];
        auto it        = mTasks.find(id);
        auto placement = mPlacements.placements.find(id);
        if (placement == mPlacements.placements.end()) {
            bool selected = mHighlightTask == it->first;
            mTasks.erase(it);
            if (selected) {
                mHighlightTask = 0;
                rebuildOverlay();
            }
            continue;
        }
        auto&       task = it->second;
        auto const& p    = placement->second;
        if (!p.enabled) {
            if (!task.stale) {
                task.stale = true;
                if (task.placementId == mHighlightTask) rebuildOverlay();
            }
            continue;
        }
        if (task.contentRevision != p.contentRevision || task.origin != p.origin || task.dimension != p.dimensionId
            || task.asset != p.asset.get() || task.rotation != p.rotation || task.mirror != p.mirror
            || task.enabled != p.enabled || (task.layerOnly && task.viewRevision != mView.revision)) {
            if (!task.running) {
                if (!task.stale) {
                    task.stale = true;
                    if (task.placementId == mHighlightTask) rebuildOverlay();
                }
                continue;
            }
            bool layers = task.layerOnly;
            auto id     = task.placementId;
            (void)startTask(id, layers);
            return;
        }
        if (!task.running || !p.enabled || !p.asset) continue;
        auto total = task.scan.size();
        size_t steps = 0;
        while (task.building && task.cursor < total && steps++ < 256 && budget.available()) {
            auto [leaf, expected] = task.scan.at(task.cursor++);
            if (!leaf || !expected || (task.layerOnly && !mView.layerRange.containsPosition(expected->world))) continue;
            auto key = util::encodePosKey(expected->world);
            auto previous = task.cells.find(key);
            
            if (previous != task.cells.end() && previous->second.expected
                && !previous->second.expected->isAir()) continue;
            VerificationCell cell{expected->block, leaf->id};
            evaluate(p.dimensionId, expected->world, cell);
            if (previous == task.cells.end()) task.keys.push_back(key);
            task.cells.insert_or_assign(key, cell);
            publishOverlay(task, expected->world, cell);
        }
        task.building = task.cursor < total;
        steps = 0;
        while (!task.building && !task.keys.empty() && steps++ < 256 && budget.available()) {
            if (task.pollCursor >= task.keys.size()) task.pollCursor = 0;
            auto  key  = task.keys[task.pollCursor++];
            auto& cell = task.cells.at(key);
            auto  pos  = util::decodePosKey(key);
            if (evaluate(p.dimensionId, pos, cell)) publishOverlay(task, pos, cell);
        }
    }
}
VerificationReport VerifierService::report(uint32_t id, BlockPos const& near, size_t limit) const {
    VerificationReport result;
    result.placementId = id;
    auto found         = mTasks.find(id);
    if (found == mTasks.end()) return result;
    auto const& task = found->second;
    result.running   = task.running;
    result.building  = task.building;
    result.layerOnly = task.layerOnly;
    result.stale     = task.stale;
    result.scanned   = task.cursor;
    auto p           = mPlacements.placements.find(id);
    if (p != mPlacements.placements.end() && p->second.asset) result.total = task.scan.size();
    std::map<std::string, VerificationGroup>      groups;
    std::unordered_map<Block const*, std::string> labels;
    auto                                          label = [&](Block const* block) -> std::string const& {
        auto [it, inserted] = labels.try_emplace(block);
        if (inserted) it->second = verificationBlockLabel(block);
        return it->second;
    };
    auto distance = [&](VerificationLocation const& value) {
        long double x = static_cast<long double>(value.position.x) - near.x;
        long double y = static_cast<long double>(value.position.y) - near.y;
        long double z = static_cast<long double>(value.position.z) - near.z;
        return x * x + y * y + z * z;
    };
    auto nearer = [&](VerificationLocation const& a, VerificationLocation const& b) {
        auto da = distance(a), db = distance(b);
        return da != db ? da < db : util::encodePosKey(a.position) < util::encodePosKey(b.position);
    };
    for (auto const& [key, cell] : task.cells) {
        ++result.counts[static_cast<size_t>(cell.status)];
        if (cell.status == VerificationStatus::Unknown || cell.status == VerificationStatus::Matched) continue;
        auto  expected = label(cell.expected), actual = label(cell.observed);
        auto  groupId  = std::to_string(static_cast<int>(cell.status)) + ":" + expected + " -> " + actual;
        auto& group    = groups[groupId];
        group.id       = groupId;
        group.expected = expected;
        group.actual   = actual;
        group.status   = cell.status;
        group.ignored  = task.ignored.contains(groupId);
        ++group.count;
        if (group.ignored) ++result.ignored;
        else if (limit != 0) {
            VerificationLocation location{util::decodePosKey(key), cell.status, groupId};
            if (result.nearby.size() < limit) {
                result.nearby.push_back(std::move(location));
                std::push_heap(result.nearby.begin(), result.nearby.end(), nearer);
            } else if (nearer(location, result.nearby.front())) {
                std::pop_heap(result.nearby.begin(), result.nearby.end(), nearer);
                result.nearby.back() = std::move(location);
                std::push_heap(result.nearby.begin(), result.nearby.end(), nearer);
            }
        }
    }
    std::sort_heap(result.nearby.begin(), result.nearby.end(), nearer);
    for (auto& [key, group] : groups) result.groups.push_back(std::move(group));
    return result;
}
} 
