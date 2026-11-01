#include "VerifierService.h"
#include "VerifierBlockListener.h"
#include "levischematic/schematic/FakeWorld.h"
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
    editor::ViewState const&
)
: mState(state),
  mPlacements(placements) {}
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
    mSpecs.clear();
    mBuildPlacement = 0;
    mRevision       = 0;
    mBuilding       = false;
    mPolling.reset();
}
void VerifierService::refresh() {
    if (mRevision != mPlacements.revision) mRefreshRequested = true;
}
void VerifierService::refresh(BlockSource&) { refresh(); }
void VerifierService::handleBlockChanged(BlockSource& source, BlockPos const& pos, Block const&) {
    auto key = util::makeWorldBlockKey(static_cast<int>(source.getDimensionId()), pos);
    if (mState.find(key.dimensionId, pos) && mDirtySet.insert(key).second) mDirty.push_back(key);
}
bool VerifierService::evaluate(int dimension, BlockPos const& pos, VerificationCell& cell) {
    Block const* actual = nullptr;
    auto         source = mSources.find(dimension);
    if (source != mSources.end() && source->second->mOwnerThreadID.get() == std::this_thread::get_id()
        && source->second->hasChunksAt(pos, 0, false))
        actual = &source->second->getBlock(pos);
    return cell.observe(actual, [this](Block const* expected, Block const* current) {
        return compare(*expected, *current);
    });
}
VerificationStatus VerifierService::compare(Block const& expected, Block const& actual) {
    if (actual.isAir()) return VerificationStatus::MissingBlock;
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
void VerifierService::insert(placement::PlacementInstance const& placement, BlockPos pos, Block const* block) {
    if (!block || block->isAir()) return;
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
    if (it == mState.chunks.end()) return;
    auto cell = it->second.cells.find(key.posKey);
    if (cell == it->second.cells.end()) return;
    if (!evaluate(key.dimensionId, pos, cell->second)) return;
    it->second.revision = ++mState.revision;
}
void VerifierService::tick(util::WorkBudget const& budget) {
    if (mRevision != mPlacements.revision || mRefreshRequested) {
        mRevision         = mPlacements.revision;
        mRefreshRequested = false;
        mState.clearing   = true;
        ++mState.epoch;
        ++mState.revision;
        mBuildPlacement  = mPlacements.order.size();
        mBlockCursor     = 0;
        mOverrideStarted = false;
        mBuilding        = true;
        mPolling.reset();
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
        auto found = mPlacements.placements.find(mPlacements.order[mBuildPlacement - 1]);
        if (found == mPlacements.placements.end() || !found->second.enabled || !found->second.renderEnabled
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
                insert(placement, util::decodePosKey(key), value.block);
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
            if (placement.overrides.contains(util::encodePosKey(pos))) continue;
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
    while (!mState.clearing && !mDirty.empty() && budget.available()) {
        auto key = mDirty.front();
        mDirty.pop_front();
        mDirtySet.erase(key);
        update(key);
    }

    auto   pollTime = VerificationPollSchedule::Clock::now();
    size_t count    = 0;
    while (!mState.clearing && !mBuilding && count++ < 512 && budget.available()) {
        auto index = mPolling.next(mKeys.size(), pollTime);
        if (!index) break;
        update(mKeys[*index]);
    }
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


    std::erase_if(
        mSources,


        [&](auto const& item) { return item.second == &source; }
    );
    mRefreshRequested = true;
}


} 
