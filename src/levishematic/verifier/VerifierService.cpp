#include "VerifierService.h"

#include "levishematic/LeviShematic.h"
#include "levishematic/util/PositionUtils.h"
#include "levishematic/verifier/VerifierBlockListener.h"

#include "ll/api/service/Bedrock.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/world/Container.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/dimension/Dimension.h"

namespace levishematic::verifier {
namespace {

auto& getLogger() {
    return levishematic::LeviShematic::getInstance().getSelf().getLogger();
}

bool matchesContainerSnapshot(
    BlockEntitySnapshot const& expected,
    BlockSource&               source,
    BlockPos const&            pos
) {
    return true; //暂时不考虑容器对比，所以先直接返回true，保留代码，以备后续
    if (!expected.container.has_value()) {
        return true;
    }

    auto* blockActor = source.getBlockEntity(pos);
    if (!blockActor) {
        return false;
    }

    auto* container = blockActor->getContainer();
    if (!container) {
        return false;
    }

    auto slots = container->getSlots();
    std::unordered_map<int, ContainerSlotSnapshot const*> expectedSlots;
    expectedSlots.reserve(expected.container->slots.size());
    for (auto const& expectedSlot : expected.container->slots) {
        if (expectedSlot.slot < 0 || expectedSlot.slot >= static_cast<int>(slots.size())) {
            return false;
        }

        expectedSlots[expectedSlot.slot] = &expectedSlot;

        auto const* item = slots[expectedSlot.slot];
        if (!item || item->isNull()) {
            return false;
        }

        if (item->getFullNameHash().getHash() != expectedSlot.itemNameHash) {
            return false;
        }

        if (static_cast<int>(item->mCount) != expectedSlot.count) {
            return false;
        }
    }

    for (int slot = 0; slot < static_cast<int>(slots.size()); ++slot) {
        auto const* item = slots[slot];
        if (!item || item->isNull()) {
            continue;
        }

        if (!expectedSlots.contains(slot)) {
            return false;
        }
    }

    return true;
}

} // namespace

VerifierService::VerifierService(
    VerifierState&                   state,
    placement::PlacementState const& placementState,
    render::ProjectionProjector&     projector
)
    : mState(state)
    , mPlacementState(placementState)
    , mProjector(projector)
    , mPlacementCache(std::make_unique<placement::PlacementProjectionCache>()) {}

VerifierService::~VerifierService() {
    detachFromRuntime();
}

void VerifierService::handleBlockChanged(BlockSource& source, BlockPos const& pos, Block const& block) {
    syncExpectedBlocks();

    auto dimensionId = static_cast<int>(source.getDimensionId());
    auto expectedIt  = mExpectedBlocksByKey.find(util::makeWorldBlockKey(dimensionId, pos));
    if (expectedIt == mExpectedBlocksByKey.end()) {
        return;
    }

    updateStatus(dimensionId, pos, evaluateBlock(expectedIt->second, source, block));
    mProjector.rebuild(mPlacementState, mState);
    mProjector.triggerRebuildForPosition(dimensionId, pos, resolveCoordinator(source));
}

void VerifierService::refresh() {
    syncExpectedBlocks();
    clearStatuses();
    mProjector.rebuild(mPlacementState, mState);
}

void VerifierService::refresh(BlockSource& source) {
    syncExpectedBlocks();

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

    for (auto const& [worldKey, expected] : mExpectedBlocksByKey) {
        if (worldKey.dimensionId != dimensionId) {
            continue;
        }

        auto const& block = source.getBlock(expected.pos);
        updateStatus(dimensionId, expected.pos, evaluateBlock(expected, source, block));
    }

    mProjector.rebuildAndRefresh(mPlacementState, mState, resolveCoordinator(source));
}

void VerifierService::handleJoinLevel() {
    attachToRuntime();
    if (auto* source = resolveCurrentBlockSource()) {
        refresh(*source);
    } else {
        refresh();
    }
}

void VerifierService::handleExitLevel() {
    detachFromRuntime();
    clearStatuses();
}

void VerifierService::handleDimensionChanged() {
    ensureRuntimeBindings();
    if (auto* source = resolveCurrentBlockSource()) {
        refresh(*source);
    } else {
        refresh();
    }
}

void VerifierService::ensureRuntimeBindings() {
    attachToRuntime();
}

void VerifierService::clear() {
    clearStatuses();
    mExpectedBlocksByKey.clear();
    mExpectedPlacementsRevision = 0;
    mPlacementCache->clear();
}

void VerifierService::attachToRuntime() {
    auto level = ll::service::getLevel();
    if (!level) {
        return;
    }

    if (!mListener) {
        mListener = std::make_unique<levishematic::verifier_block_listener::VerifierBlockListener>(*this);
    }

    for (int dimId : {0, 1, 2}) {
        auto dimensionRef = level->getDimension(dimId);
        auto dimension    = dimensionRef.lock();
        if (!dimension) {
            continue;
        }

        auto& source = dimension->getBlockSourceFromMainChunkSource();
        if (auto existing = mSourcesByDimension.find(dimId); existing != mSourcesByDimension.end()) {
            if (existing->second == &source) {
                continue;
            }
            if (existing->second) {
                existing->second->removeListener(*mListener);
            }
        }

        source.addListener(*mListener);
        mSourcesByDimension[dimId] = &source;
    }
}

void VerifierService::detachFromRuntime() {
    if (mListener) {
        for (auto const& [dimId, source] : mSourcesByDimension) {
            (void)dimId;
            if (source) {
                source->removeListener(*mListener);
            }
        }
    }

    mSourcesByDimension.clear();
    mListener.reset();
}

VerificationStatus VerifierService::evaluateBlock(
    ExpectedBlockSnapshot const& expected,
    BlockSource&                 source,
    Block const&                 block
) const {
    if (block.isAir()) {
        return VerificationStatus::MissingBlock;
    }

    auto blockNameHash = block.getBlockType().mNameInfo->mFullName->getHash();
    if (blockNameHash != expected.compareSpec.nameHash) {
        return VerificationStatus::BlockMismatch;
    }

    for (auto const& state : expected.compareSpec.exactStates) {
        auto value = block.getState<int>(state.stateId);
        if (!value.has_value() || *value != state.value) {
            return VerificationStatus::PropertyMismatch;
        }
    }

    if (expected.compareSpec.compareContainer && expected.blockEntity.has_value()
        && !matchesContainerSnapshot(*expected.blockEntity, source, expected.pos)) {
        return VerificationStatus::PropertyMismatch;
    }

    return VerificationStatus::Matched;
}

void VerifierService::syncExpectedBlocks() {
    if (mExpectedPlacementsRevision == mPlacementState.revision) {
        return;
    }

    mExpectedBlocksByKey.clear();
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
        for (auto const& [worldKey, expected] : projection.expectedBlocksByKey) {
            mExpectedBlocksByKey[worldKey] = expected;
        }
    }

    mExpectedPlacementsRevision = mPlacementState.revision;
}

void VerifierService::clearStatuses() {
    if (mState.statusByKey.empty()) {
        return;
    }

    mState.statusByKey.clear();
    ++mState.revision;
}

std::shared_ptr<RenderChunkCoordinator> VerifierService::resolveCoordinator(BlockSource const& source) const {
    auto client = ll::service::getClientInstance();
    if (!client || !client->getLevelRenderer()) {
        return nullptr;
    }

    auto dimId = static_cast<int>(source.getDimensionId());
    return client->getLevelRenderer()->mRenderChunkCoordinators->at(dimId);
}

BlockSource* VerifierService::resolveCurrentBlockSource() const {
    auto client = ll::service::getClientInstance();
    if (!client) {
        return nullptr;
    }

    auto* player = client->getLocalPlayer();
    if (!player) {
        return nullptr;
    }

    auto dimId = static_cast<int>(player->getDimensionId());
    auto it    = mSourcesByDimension.find(dimId);
    return it == mSourcesByDimension.end() ? nullptr : it->second;
}

void VerifierService::updateStatus(int dimensionId, BlockPos const& pos, VerificationStatus status) {
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

} // namespace levishematic::verifier
