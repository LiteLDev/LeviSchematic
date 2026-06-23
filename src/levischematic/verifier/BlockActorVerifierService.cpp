#include "BlockActorVerifierService.h"

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/actor/BlockActor.h"

namespace levischematic::verifier {

BlockActorVerifierService::BlockActorVerifierService(
    BlockActorVerifierState&                 state,
    placement::PlacementState const&         placementState,
    editor::ViewState const&                 viewState,
    render::BlockActorProjectionProjector&   projector
)
    : mState(state)
    , mPlacementState(placementState)
    , mViewState(viewState)
    , mProjector(projector)
    , mPlacementCache(std::make_unique<placement::PlacementProjectionCache>()) {}

BlockActorVerifierService::~BlockActorVerifierService() = default;

void BlockActorVerifierService::handleBlockChanged(BlockSource& source, BlockPos const& pos, Block const& block) {
    syncExpectedBlockActors();

    auto dimensionId = static_cast<int>(source.getDimensionId());
    auto expectedIt  = mExpectedBlockActorsByKey.find(util::makeWorldBlockKey(dimensionId, pos));
    if (expectedIt == mExpectedBlockActorsByKey.end()) {
        return;
    }

    updateStatus(dimensionId, pos, evaluateBlockActor(expectedIt->second, source, block));
    mProjector.rebuild(mPlacementState, mState, mViewState);
    mProjector.triggerRebuildForPosition(dimensionId, pos, resolveCoordinator(source));
}

void BlockActorVerifierService::refresh() {
    syncExpectedBlockActors();
    clearStatuses();
    mProjector.rebuild(mPlacementState, mState, mViewState);
}

void BlockActorVerifierService::refresh(BlockSource& source) {
    syncExpectedBlockActors();

    auto dimensionId = static_cast<int>(source.getDimensionId());
    bool removedAny  = false;
    for (auto it = mState.statusByKey.begin(); it != mState.statusByKey.end();) {
        if (it->first.dimensionId == dimensionId) {
            it = mState.statusByKey.erase(it);
            removedAny = true;
            continue;
        }
        ++it;
    }
    if (removedAny) {
        ++mState.revision;
    }

    for (auto const& [worldKey, expected] : mExpectedBlockActorsByKey) {
        if (worldKey.dimensionId != dimensionId) {
            continue;
        }

        auto const& block = source.getBlock(expected.pos);
        updateStatus(dimensionId, expected.pos, evaluateBlockActor(expected, source, block));
    }

    mProjector.rebuildAndRefresh(mPlacementState, mState, mViewState, resolveCoordinator(source));
}

void BlockActorVerifierService::clear() {
    clearStatuses();
    mExpectedBlockActorsByKey.clear();
    mExpectedPlacementsRevision = 0;
    mExpectedViewRevision       = 0;
    mPlacementCache->clear();
}

VerificationStatus BlockActorVerifierService::evaluateBlockActor(
    BlockActorExpectedSnapshot const& expected,
    BlockSource&                      source,
    Block const&                      block
) const {
    (void)expected;
    if (block.isAir()) {
        return VerificationStatus::MissingBlock;
    }

    auto* blockActor = source.getBlockEntity(expected.pos);
    if (!blockActor) {
        return VerificationStatus::MissingBlock;
    }

    return VerificationStatus::Unknown;
}

void BlockActorVerifierService::syncExpectedBlockActors() {
    if (mExpectedPlacementsRevision == mPlacementState.revision && mExpectedViewRevision == mViewState.revision) {
        return;
    }

    mExpectedBlockActorsByKey.clear();
    for (auto placementId : mPlacementState.order) {
        auto placementIt = mPlacementState.placements.find(placementId);
        if (placementIt == mPlacementState.placements.end()) {
            continue;
        }

        auto const& placement = placementIt->second;
        if (!placement.enabled || !placement.renderEnabled || !placement.asset) {
            continue;
        }

        auto projection = mPlacementCache->view(placement);
        for (auto const& entry : projection.blockActorEntries) {
            if (!entry.block || !entry.blockActor || !mViewState.layerRange.contains(entry.pos.y)) {
                continue;
            }

            auto worldKey = util::makeWorldBlockKey(placement.dimensionId, entry.pos);
            mExpectedBlockActorsByKey[worldKey] = BlockActorExpectedSnapshot{
                .dimensionId = placement.dimensionId,
                .pos         = entry.pos,
                .renderBlock = entry.block,
                .blockActor  = entry.blockActor,
                .rendererId  = entry.rendererId,
                .placementId = placement.id,
            };
        }
    }

    mExpectedPlacementsRevision = mPlacementState.revision;
    mExpectedViewRevision       = mViewState.revision;

    bool removedAnyStatus = false;
    for (auto it = mState.statusByKey.begin(); it != mState.statusByKey.end();) {
        if (!mExpectedBlockActorsByKey.contains(it->first)) {
            it = mState.statusByKey.erase(it);
            removedAnyStatus = true;
            continue;
        }
        ++it;
    }
    if (removedAnyStatus) {
        ++mState.revision;
    }
}

void BlockActorVerifierService::clearStatuses() {
    if (mState.statusByKey.empty()) {
        return;
    }

    mState.statusByKey.clear();
    ++mState.revision;
}

void BlockActorVerifierService::updateStatus(int dimensionId, BlockPos const& pos, VerificationStatus status) {
    auto key = util::makeWorldBlockKey(dimensionId, pos);
    if (status == VerificationStatus::Unknown) {
        if (mState.statusByKey.erase(key) > 0) {
            ++mState.revision;
        }
        return;
    }

    auto it = mState.statusByKey.find(key);
    if (it != mState.statusByKey.end() && it->second == status) {
        return;
    }

    mState.statusByKey[key] = status;
    ++mState.revision;
}

std::shared_ptr<RenderChunkCoordinator> BlockActorVerifierService::resolveCoordinator(BlockSource const& source) const {
    auto client = ll::service::getClientInstance();
    if (!client || !client->getLevelRenderer()) {
        return nullptr;
    }

    auto dimId = static_cast<int>(source.getDimensionId());
    return client->getLevelRenderer()->mRenderChunkCoordinators->at(dimId);
}

} // namespace levischematic::verifier
