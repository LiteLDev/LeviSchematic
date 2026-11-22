#include "levischematic/interaction/survival/service/SurvivalService.h"
#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/interaction/worldedit/runtime/PlacementInteraction.h"
#include "levischematic/presentation/ui/model/Ui.h"
#include "levischematic/shared/i18n/I18n.h"
#include "levischematic/verifier/service/VerificationTask.h"
#include "levischematic/workspace/config/ConfigStore.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/item/Item.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/ShapeType.h"
#include "mc/world/level/dimension/Dimension.h"
#include <Windows.h>
#include <chrono>

namespace levischematic::app {
namespace {
uint64_t nowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count()
    );
}
HitResult   worldTrace(LocalPlayer& player) { return worldedit::traceWorld(player); }
std::string positionText(BlockPos const& p) {
    return std::to_string(p.x) + ", " + std::to_string(p.y) + ", " + std::to_string(p.z);
}
} // namespace
std::optional<placement::ProjectionHit> SurvivalService::target(LocalPlayer& player) {
    auto start = player.getHeadPos(), dir = player.getViewVector(1);
    return placement::traceProjection(
        mPlacements.orderedPlacements(),
        static_cast<int>(player.getDimensionId()),
        mPlacements.viewState().layerRange,
        {start.x, start.y, start.z},
        {dir.x, dir.y, dir.z},
        player.getPickRange()
    );
}
bool SurvivalService::hasTarget(IClientInstance& client) {
    auto player = client.getLocalPlayer();
    if (!player) return false;
    auto lock = mPlacements.lockPlacements();
    auto hit  = target(*player);
    if (configStore().snapshot().diagnostics) {
        auto now = nowMs();
        if (now - mLastProbe >= 1000) {
            mLastProbe    = now;
            auto selected = mPlacements.requireSelectedPlacement();
            LeviSchematic::getInstance().getSelf().getLogger().info(
                "Survival target: dimension={} eye={} reach={} origin={} hit={}",
                static_cast<int>(player->getDimensionId()),
                player->getHeadPos().toString(),
                player->getPickRange(),
                selected ? selected.value()->origin.toString() : "none",
                hit ? hit->position.toString() : "none"
            );
        }
    }
    return hit.has_value();
}
void SurvivalService::notify(LocalPlayer& player, std::string text) {
    if (!configStore().snapshot().diagnostics) return;
    auto now = nowMs();
    if (text != mMessage || now - mLastMessage >= 2000) {
        player.displayClientMessage(text, std::nullopt);
        LeviSchematic::getInstance().getSelf().getLogger().info("Survival: {}", text);
        mLastMessage = now;
    }
    mMessage = std::move(text);
}
void SurvivalService::reset() {
    mOperation.reset();
    mTarget.reset();
    mUseHeld       = false;
    mUseRequested  = false;
    mInfoHeld      = false;
    mObservedMatch = false;
    mHud.clear();
    mTargetHudLines = 0;
    mMaterials.reset();
}
void SurvivalService::pick(IClientInstance& client) {
    auto player = client.getLocalPlayer();
    if (!player) return;
    auto lock = mPlacements.lockPlacements();
    auto hit  = target(*player);
    if (!hit) return;
    auto p = mPlacements.findPlacement(hit->owner);
    if (!p) return;
    auto expected = placement::expectedAt(*p, hit->position);
    if (!expected) return;
    auto items = survival::resolveBlockItems(*p, *expected);
    if (!items.pick) {
        notify(*player, items.unsupported);
        return;
    }
    auto result = survival::selectBuildItem(*player, items.pick, configStore().snapshot().survival.hotbarSlots);
    switch (result) {
    case survival::InventoryResult::Ready:
        notify(*player, tr("msg.survival.selected", items.pick.getDescriptionName()));
        break;
    case survival::InventoryResult::Submitted:
        notify(*player, tr("msg.survival.inventory_submitted"));
        break;
    case survival::InventoryResult::Busy:
        notify(*player, tr("msg.survival.inventory_busy"));
        break;
    case survival::InventoryResult::Missing:
        notify(*player, tr("msg.survival.inventory_missing", items.pick.getDescriptionName()));
        break;
    case survival::InventoryResult::NoSlot:
        notify(*player, tr("msg.survival.no_slot"));
        break;
    default:
        notify(*player, tr("msg.survival.unsupported_item"));
        break;
    }
}
bool SurvivalService::restrictPlacement(LocalPlayer& player, BlockPos const& pos, unsigned char face) {
    if (!configStore().snapshot().survival.placementRestriction) return false;
    auto lock = mPlacements.lockPlacements();
    auto list = mPlacements.orderedPlacements();
    auto dim  = static_cast<int>(player.getDimensionId());
    if (!placement::nearProjection(list, dim, pos)) return false;
    auto expected = placement::projectionAt(list, dim, pos);
    auto deny     = [&](std::string reason) {
        notify(player, tr("msg.survival.placement_blocked") + reason);
        return true;
    };
    if (!expected || !expected->block || expected->block->isAir())
        return deny(tr("msg.survival.expected_air_or_unknown"));
    if (!mPlacements.viewState().layerRange.containsPosition(pos)) return deny(tr("msg.survival.outside_render_layer"));
    auto p    = mPlacements.findPlacement(expected->owner);
    auto cell = p ? placement::expectedAt(*p, pos) : std::nullopt;
    if (!cell) return deny(tr("msg.survival.unknown_target"));
    auto        items = survival::resolveBlockItems(*p, *cell);
    auto const& held  = player.getSelectedItem();
    if (!items.pick || !held || survival::itemKey(held) != survival::itemKey(items.pick))
        return deny(tr("msg.survival.wrong_item"));
    auto trace  = worldTrace(player);
    auto render = held.getBlockForRendering();
    if (!render || trace.mType != HitResultType::Tile || face >= 6)
        return deny(tr("msg.survival.cannot_predict_placement"));
    Vec3        click     = trace.mPos - Vec3(trace.mBlock);
    auto const& predicted = render->getBlockType().getPlacementBlock(player, pos, face, click, held.getAuxValue());
    if (!survival::placementOrientationMatches(*expected->block, predicted))
        return deny(tr("msg.survival.orientation_or_state"));
    return false;
}
void SurvivalService::advance(LocalPlayer& player, uint64_t now) {
    auto cfg = configStore().snapshot().survival;
    if (!cfg.easyPlace) {
        mOperation.reset();
        mTarget.reset();
        return;
    }
    if (!mTarget) {
        if (!mUseRequested && !(mUseHeld && cfg.holdToPlace)) return;
        if (now < mNextUse) return;
        mUseRequested = false;
        auto hit      = target(player);
        if (!hit) return;
        mTarget = hit->position;
        mOwner  = hit->owner;
        auto p  = mPlacements.findPlacement(mOwner);
        if (!p) {
            mTarget.reset();
            return;
        }
        mPlacementRevision = p->revision;
        mOperation.begin(now);
        mObservedMatch = false;
    }
    auto finish = [&](std::string text) {
        notify(player, std::move(text));
        mTarget.reset();
        mOperation.reset();
        mNextUse = now + 250;
    };
    if (mOperation.expired(now)) {
        finish(tr("msg.survival.placement_unconfirmed"));
        return;
    }
    auto p = mPlacements.findPlacement(mOwner);
    if (!p || !p->enabled || p->revision != mPlacementRevision
        || p->dimensionId != static_cast<int>(player.getDimensionId())) {
        finish(tr("msg.survival.target_changed"));
        return;
    }
    auto expected = placement::expectedAt(*p, *mTarget);
    if (!expected || !expected->block || !mPlacements.viewState().layerRange.containsPosition(*mTarget)) {
        finish(tr("msg.survival.target_outside_layer"));
        return;
    }
    auto& source = player.getDimension().getBlockSourceFromMainChunkSource();
    if (!source.hasChunksAt(*mTarget, 0, false)) {
        finish(tr("msg.survival.chunk_unavailable"));
        return;
    }
    auto status = survival::compareBlocks(expected->block, &source.getBlock(*mTarget));
    if (mOperation.phase == survival::Phase::WaitingWorld) {
        if (status == verifier::VerificationStatus::Matched) {
            if (!mObservedMatch) {
                mObservedMatch = true;
                mSwapTime      = now;
            } else if (now - mSwapTime >= 250) finish(tr("msg.survival.placement_observed_server_validation"));
        } else if (mObservedMatch) finish(tr("msg.survival.placement_corrected_or_rejected"));
        return;
    }
    if (status == verifier::VerificationStatus::Matched) {
        finish(tr("msg.survival.already_matches"));
        return;
    }
    if (status != verifier::VerificationStatus::MissingBlock) {
        finish(tr("msg.survival.clear_or_repair_target_manually"));
        return;
    }
    auto items = survival::resolveBlockItems(*p, *expected);
    if (!items.unsupported.empty() || !items.pick || items.rule.secondary) {
        finish(items.unsupported.empty() ? tr("msg.survival.place_multi_block_item_from_its") : items.unsupported);
        return;
    }
    if (mOperation.phase == survival::Phase::Selecting) {
        auto result = survival::selectBuildItem(player, items.pick, cfg.hotbarSlots);
        if (result == survival::InventoryResult::Busy) return;
        if (result == survival::InventoryResult::Missing) {
            finish(tr("msg.survival.missing_item"));
            return;
        }
        if (result == survival::InventoryResult::NoSlot || result == survival::InventoryResult::Unsupported) {
            finish(tr("msg.survival.no_usable_hotbar_slot"));
            return;
        }
        if (result == survival::InventoryResult::Submitted) {
            mOperation.phase = survival::Phase::WaitingInventory;
            mSwapTime        = now;
            return;
        }
        mOperation.phase = survival::Phase::Ready;
    }
    if (mOperation.phase == survival::Phase::WaitingInventory) {
        if (now - mSwapTime < static_cast<unsigned>(cfg.swapDelayMs)) return;
        if (!player.getSelectedItem() || survival::itemKey(player.getSelectedItem()) != survival::itemKey(items.pick)) {
            finish(tr("msg.survival.inventory_selection_not_confirmed"));
            return;
        }
        mOperation.phase = survival::Phase::Ready;
    }
    std::string reason;
    if (!worldedit::buildAtAim(player, *mTarget, *expected->block, reason)) {
        finish(std::move(reason));
        return;
    }
    mOperation.phase = survival::Phase::WaitingWorld;
    mSwapTime        = now;
}
void SurvivalService::tick(IClientInstance& client, util::WorkBudget const& budget) {
    auto player = client.getLocalPlayer();
    if (!player) {
        reset();
        return;
    }
    int dimension = static_cast<int>(player->getDimensionId());
    if (mDimension != dimension) {
        reset();
        mDimension = dimension;
    }
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    if (process != GetCurrentProcessId() || !client.getMouseGrabbed() || !client.isInWorldAndNotShowingAnyMenuScreens()
        || ui::isOpen()) {
        mUseHeld      = false;
        mUseRequested = false;
        mInfoHeld     = false;
        mTarget.reset();
        mOperation.reset();
    }
    auto lock = mPlacements.lockPlacements();
    auto now  = nowMs();
    advance(*player, now);
    if (configStore().snapshot().survival.materialHud && mMaterials.source() == MaterialSource::None)
        mMaterials.request(false);
    mMaterials.tick(*player, budget);
    if (now < mNextHud) return;
    mNextHud = now + 100;
    mHud.clear();
    if (mInfoHeld) {
        auto hit = target(*player);
        if (hit) {
            auto& source = player->getDimension().getBlockSourceFromMainChunkSource();
            mHud.push_back("Projection #" + std::to_string(hit->owner) + " @ " + positionText(hit->position));
            mHud.push_back("Expected: " + verifier::verificationBlockLabel(hit->block));
            mHud.push_back(
                "Actual: "
                + verifier::verificationBlockLabel(
                    source.hasChunksAt(hit->position, 0, false) ? &source.getBlock(hit->position) : nullptr
                )
            );
        } else mHud.push_back(tr("msg.survival.no_projection_target"));
    }
    auto cfg        = configStore().snapshot().survival;
    mTargetHudLines = mHud.size();
    if (cfg.materialHud) {
        auto report = mMaterials.report();
        mHud.push_back(report.building ? tr("msg.survival.materials_scanning") : tr("msg.survival.materials"));
        size_t count = 0;
        for (auto const& row : report.rows)
            if (row.counts.remaining() && !mMaterials.ignored(row.item) && count++ < 5)
                mHud.push_back(
                    row.label + ": " + std::to_string(row.counts.toGather()) + " / "
                    + std::to_string(row.counts.available) + " / " + std::to_string(row.counts.total)
                );
    }
}
} // namespace levischematic::app
