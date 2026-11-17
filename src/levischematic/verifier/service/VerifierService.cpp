#include "levischematic/verifier/service/VerifierService.h"
#include "levischematic/schematic/placement/model/PlacementGeometry.h"
#include "levischematic/schematic/placement/query/ProjectionQuery.h"
#include "levischematic/schematic/world/FakeWorld.h"
#include "levischematic/verifier/model/VerificationPresence.h"
#include "levischematic/verifier/runtime/VerifierBlockListener.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/states/VanillaBlockStateTransformUtils.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/dimension/Dimension.h"
#include <thread>

namespace levischematic::verifier {
VerifierService::VerifierService(
    VerifierState&                   state,
    placement::PlacementState const& placements,
    editor::ViewState const&         view
)
: mState(state),
  mPlacements(placements),
  mView(view) {}
VerifierService::~VerifierService() { detachFromRuntime(); }
void VerifierService::handleJoinLevel(ClientInstance& client) {
    attachToRuntime(client);
    mRefreshRequested = true;
}
void VerifierService::handleExitLevel() {
    detachFromRuntime();
    clear();
}
void VerifierService::ensureRuntimeBindings(ClientInstance& client) { attachToRuntime(client); }
void VerifierService::clear() {
    mState.chunks.clear();
    ++mState.epoch;
    ++mState.revision;
    mState.clearing = false;
    mKeys.clear();
    mDirty.clear();
    mDirtySet.clear();
    mChangeStats = {};
    mSpecs.clear();
    mBuildPlacement = 0;
    mRevision       = 0;
    mBuilding       = false;
    mPolling.reset();
    mTasks.clear();
    mState.taskOverlay.reset();
    mRetiredOverlays.clear();
    mOverlayRebuild = false;
    mHighlightTask  = 0;
    mAirSweeping    = false;
    mAirPlacement   = 0;
    mAirCursor      = 0;
    mAirNext        = {};
}
void VerifierService::refresh() {
    if (mRevision != mPlacements.revision) mRefreshRequested = true;
}
void VerifierService::refresh(BlockSource&) { refresh(); }
void VerifierService::handleBlockChanged(BlockSource& source, BlockPos const& pos, Block const&) {
    auto key     = util::makeWorldBlockKey(static_cast<int>(source.getDimensionId()), pos);
    bool tracked = mState.find(key.dimensionId, pos) != nullptr || insideProjection(key.dimensionId, pos);
    if (!tracked) {
        for (auto const& [id, task] : mTasks) {
            auto p = mPlacements.projections.find(id);
            if (task.running && p != mPlacements.projections.end() && p->second.dimensionId == key.dimensionId
                && task.cells.contains(key.posKey)) {
                tracked = true;
                break;
            }
        }
    }
    if (tracked && mDirtySet.insert(key).second) {
        if (mDirty.empty()) mDirtySince = util::WorkBudget::Clock::now();
        mDirty.push_back(key);
    }
}
Block const* VerifierService::observedBlock(int dimension, BlockPos const& pos) const {
    auto source = mSources.find(dimension);
    if (source != mSources.end() && source->second->mOwnerThreadID.get() == std::this_thread::get_id()
        && source->second->hasChunksAt(pos, 0, false))
        return &source->second->getBlock(pos);
    return nullptr;
}
bool VerifierService::evaluate(int dimension, BlockPos const& pos, VerificationCell& cell) {
    auto* actual = observedBlock(dimension, pos);
    return cell.observe(actual, [this](Block const* expected, Block const* current) {
        return compare(*expected, *current);
    });
}
VerificationStatus VerifierService::compare(Block const& expected, Block const& actual) {
    if (auto status = classifyPresence(expected.isAir(), actual.isAir())) return *status;
    auto [it, inserted] = mSpecs.try_emplace(&expected);
    if (inserted) it->second = buildCompareSpecFromBlock(expected);
    auto const& spec = it->second;
    if (actual.getBlockType().mNameInfo->mFullName->getHash() != spec.nameHash)
        return VerificationStatus::BlockMismatch;
    if (!spec.complete) return VerificationStatus::Unknown;
    if (spec.bedDirection) {
        auto direction = readBedDirection(actual);
        if (!direction) return VerificationStatus::Unknown;
        if (*direction != *spec.bedDirection) return VerificationStatus::PropertyMismatch;
    }
    for (auto const& state : spec.exactStates) {
        auto value = actual.getState<int>(state.stateId);
        if (!value || *value != state.value) return VerificationStatus::PropertyMismatch;
    }
    return VerificationStatus::Matched;
}
void VerifierService::insert(
    placement::PlacementInstance const& placement,
    BlockPos                            pos,
    Block const*                        block,
    bool                                allowAir
) {
    if (!block || (block->isAir() && !allowAir)) return;
    auto  key           = util::makeWorldBlockKey(placement.dimensionId, pos);
    auto& chunk         = mState.chunks[{key.dimensionId, util::subChunkKeyFromWorldPos(pos.x, pos.y, pos.z)}];
    auto [it, inserted] = chunk.cells.try_emplace(key.posKey, VerificationCell{block, placement.id});
    if (!inserted) return;
    evaluate(key.dimensionId, pos, it->second);
    chunk.revision = ++mState.revision;
    mKeys.push_back(key);
}
void VerifierService::update(util::WorldBlockKey key) {
    auto pos = util::decodePosKey(key.posKey);
    auto it  = mState.chunks.find({key.dimensionId, util::subChunkKeyFromWorldPos(pos.x, pos.y, pos.z)});
    if (it == mState.chunks.end()) return trackAirAt(key);
    auto cell = it->second.cells.find(key.posKey);
    if (cell == it->second.cells.end()) return trackAirAt(key);
    if (!evaluate(key.dimensionId, pos, cell->second)) return;
    it->second.revision = ++mState.revision;
}
bool VerifierService::insideProjection(int dimension, BlockPos const& pos) const {
    for (auto const& [id, p] : mPlacements.projections) {
        if (!p.enabled || !p.renderEnabled || !p.asset || p.dimensionId != dimension) continue;
        auto size = p.asset->size;
        if (p.rotation == placement::PlacementInstance::Rotation::CW_90
            || p.rotation == placement::PlacementInstance::Rotation::CCW_90)
            std::swap(size.x, size.z);
        auto inside = [](int value, int origin, int extent) {
            return int64_t(value) >= origin && int64_t(value) < int64_t(origin) + extent;
        };
        if (inside(pos.x, p.origin.x, size.x) && inside(pos.y, p.origin.y, size.y) && inside(pos.z, p.origin.z, size.z))
            return true;
    }
    return false;
}
bool VerifierService::trackAir(placement::PlacementInstance const& p, BlockPos const& pos, Block const& air) {
    if (mState.clearing || mBuilding || mState.find(p.dimensionId, pos)) return false;
    auto* actual = observedBlock(p.dimensionId, pos);
    if (!actual || actual->isAir()) return false;
    insert(p, pos, &air, true);
    return true;
}
void VerifierService::trackAirAt(util::WorldBlockKey key) {
    if (mState.clearing || mBuilding) return;
    auto pos = util::decodePosKey(key.posKey);
    for (auto id = mPlacements.projectionOrder.rbegin(); id != mPlacements.projectionOrder.rend(); ++id) {
        auto found = mPlacements.projections.find(*id);
        if (found == mPlacements.projections.end()) continue;
        auto const& p = found->second;
        if (!p.enabled || !p.renderEnabled || !p.asset || p.dimensionId != key.dimensionId) continue;
        auto expected = placement::expectedAt(p, pos, true);
        if (!expected || !expected->block || !expected->block->isAir()) continue;
        trackAir(p, pos, *expected->block);
        return;
    }
}
void VerifierService::sweepAir(util::WorkBudget const& budget) {
    if (mState.clearing || mBuilding) return;
    auto now = VerificationPollSchedule::Clock::now();
    if (!mAirSweeping) {
        if (now < mAirNext) return;
        mAirSweeping  = true;
        mAirPlacement = 0;
        mAirCursor    = 0;
    }
    auto const& order = mPlacements.projectionOrder;
    size_t      steps = 0;
    while (mAirPlacement < order.size() && steps < 4096 && budget.available()) {
        auto found = mPlacements.projections.find(order[order.size() - 1 - mAirPlacement]);
        if (found == mPlacements.projections.end() || !found->second.enabled || !found->second.renderEnabled
            || !found->second.asset || mAirCursor >= found->second.asset->indices.size()) {
            ++mAirPlacement;
            mAirCursor = 0;
            continue;
        }
        auto const& p     = found->second;
        auto        index = p.asset->indices[mAirCursor];
        ++steps;
        if (p.overrides.empty() && index >= 0 && static_cast<size_t>(index) < p.asset->palette.size()
            && p.asset->palette[index] && !p.asset->palette[index]->isAir()) {
            ++mAirCursor;
            continue;
        }
        auto expected = placement::expectedPosition(p, mAirCursor++, true);
        if (!expected || !expected->block || !expected->block->isAir()) continue;
        trackAir(p, expected->world, *expected->block);
    }
    if (mAirPlacement >= order.size()) {
        mAirSweeping = false;
        mAirNext     = now + std::chrono::seconds{1};
    }
}
void VerifierService::tickChanges(util::WorkBudget const& budget) {
    auto start = util::WorkBudget::Clock::now();
    if (!mDirty.empty())
        mChangeStats.maxWaitMs =
            std::max(mChangeStats.maxWaitMs, std::chrono::duration<double, std::milli>(start - mDirtySince).count());
    if (mRevision != mPlacements.revision || mRefreshRequested) return;
    while (!mState.clearing && !mDirty.empty() && budget.available()) {
        auto key = mDirty.front();
        mDirty.pop_front();
        mDirtySet.erase(key);
        update(key);
        auto pos = util::decodePosKey(key.posKey);
        for (auto& [id, task] : mTasks) {
            auto p = mPlacements.projections.find(id);
            if (!task.running || p == mPlacements.projections.end() || p->second.dimensionId != key.dimensionId)
                continue;
            auto cell = task.cells.find(key.posKey);
            if (cell != task.cells.end() && evaluate(key.dimensionId, pos, cell->second))
                publishOverlay(task, pos, cell->second);
        }
        ++mChangeStats.processed;
    }
    mChangeStats.lastMs = std::chrono::duration<double, std::milli>(util::WorkBudget::Clock::now() - start).count();
}
void VerifierService::tick(util::WorkBudget const& budget) {
    placement::ensureProjectionInstances(mPlacements);
    mTaskFirst = !mTaskFirst;
    tickChanges(budget);
    if (mTaskFirst) tickTasks(budget);
    if (mRevision != mPlacements.revision || mRefreshRequested) {
        mRevision         = mPlacements.revision;
        mRefreshRequested = false;
        mState.clearing   = true;
        ++mState.epoch;
        ++mState.revision;
        mBuildPlacement  = mPlacements.projectionOrder.size();
        mBlockCursor     = 0;
        mOverrideStarted = false;
        mBuilding        = true;
        mPolling.reset();
        mAirSweeping = false;
        mAirNext     = {};
    }
    while (mState.clearing && budget.available()) {
        if (!mState.chunks.empty()) {
            auto it = mState.chunks.begin();
            if (!it->second.cells.empty()) it->second.cells.erase(it->second.cells.begin());
            else mState.chunks.erase(it);
        } else if (!mKeys.empty()) mKeys.pop_back();
        else if (!mDirty.empty()) {
            mDirtySet.erase(mDirty.front());
            mDirty.pop_front();
        } else mState.clearing = false;
    }
    while (!mState.clearing && mBuilding && budget.available()) {
        if (mBuildPlacement == 0) {
            mBuilding = false;
            mPolling.reset();
            break;
        }
        auto found = mPlacements.projections.find(mPlacements.projectionOrder[mBuildPlacement - 1]);
        if (found == mPlacements.projections.end() || !found->second.enabled || !found->second.renderEnabled
            || !found->second.asset) {
            --mBuildPlacement;
            mBlockCursor     = 0;
            mOverrideStarted = false;
            continue;
        }
        auto const& placement = found->second;
        if (!mOverrideStarted) {
            mOverride        = placement.overrides.begin();
            mOverrideStarted = true;
        }
        if (mOverride != placement.overrides.end()) {
            auto const& [key, value] = *mOverride++;
            if (value.kind == placement::OverrideEntry::Kind::SetBlock)
                if (auto pos = placement::toWorld(placement, util::decodePosKey(key)))
                    insert(placement, *pos, placement::projectedBlock(placement, value.block));
            continue;
        }
        if (mBlockCursor < placement.asset->localBlocks.size()) {
            auto const& local       = placement.asset->localBlocks[mBlockCursor++];
            auto        transformed = placement::transformLocalPos(
                local.localPos,
                placement.asset->size,
                placement.mirror,
                placement.rotation
            );
            BlockPos pos{
                placement.origin.x + transformed.x,
                placement.origin.y + transformed.y,
                placement.origin.z + transformed.z
            };
            if (placement.overrides.contains(util::encodePosKey(local.localPos))) continue;
            auto* block = VanillaBlockStateTransformUtils::transformBlock(
                *local.renderBlock,
                render::toMinecraftRotation(placement.rotation),
                render::toMinecraftMirror(placement.mirror)
            );
            insert(placement, pos, block);
            continue;
        }
        --mBuildPlacement;
        mBlockCursor     = 0;
        mOverrideStarted = false;
    }
    tickChanges(budget);
    sweepAir(budget);
    auto   pollTime = VerificationPollSchedule::Clock::now();
    size_t count    = 0;
    while (!mState.clearing && !mBuilding && count++ < 512 && budget.available()) {
        auto index = mPolling.next(mKeys.size(), pollTime);
        if (!index) break;
        update(mKeys[*index]);
    }
    if (!mTaskFirst) tickTasks(budget);
}
void VerifierService::attachToRuntime(ClientInstance& client) {
    auto* level = client.getLevel();
    if (!level) {
        return;
    }

    if (!mListener) {
        mListener = std::make_shared<levischematic::verifier_block_listener::VerifierBlockListener>();
    }

    for (int dimId : {0, 1, 2}) {
        auto dimensionRef = level->getDimension(dimId);
        auto dimension    = dimensionRef.lock();
        if (!dimension) {
            continue;
        }

        auto& source = dimension->getBlockSourceFromMainChunkSource();
        if (source.mOwnerThreadID.get() != std::this_thread::get_id()) continue;
        if (auto existing = mSources.find(dimId); existing != mSources.end()) {
            if (existing->second == &source) {
                continue;
            }
            if (existing->second) {
                existing->second->removeListener(*mListener);
            }
        }

        source.addListener(*mListener);
        mSources[dimId]   = &source;
        mRefreshRequested = true;
    }
}

void VerifierService::detachFromRuntime() {
    if (mListener) {
        for (auto const& [dimId, source] : mSources) {
            (void)dimId;
            if (source) {
                source->removeListener(*mListener);
            }
        }
    }

    mSources.clear();
    mListener.reset();
}

void VerifierService::handleSourceDestroyed(BlockSource& source) {
    std::erase_if(mSources, [&](auto const& item) { return item.second == &source; });
    mRefreshRequested = true;
}


}
