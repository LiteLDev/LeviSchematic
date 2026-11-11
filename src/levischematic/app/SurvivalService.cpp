#include "SurvivalService.h"
#include "ConfigStore.h"
#include "levischematic/LeviSchematic.h"
#include "levischematic/ui/Ui.h"
#include "levischematic/verifier/VerificationTask.h"
#include "levischematic/worldedit/PlacementInteraction.h"
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
} 
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
    mMaterials.invalidate();
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
        notify(*player, "Selected / 已选取: " + items.pick.getDescriptionName());
        break;
    case survival::InventoryResult::Submitted:
        notify(*player, "Inventory request submitted / 已提交取物请求");
        break;
    case survival::InventoryResult::Busy:
        notify(*player, "Inventory busy, retry / 背包同步中，请重试");
        break;
    case survival::InventoryResult::Missing:
        notify(*player, "Missing item / 背包缺少物品: " + items.pick.getDescriptionName());
        break;
    case survival::InventoryResult::NoSlot:
        notify(*player, "No replaceable hotbar slot / 无可替换快捷栏槽位");
        break;
    default:
        notify(*player, "Unsupported item / 不支持的物品");
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
        notify(player, "Placement blocked / 放置已阻止: " + reason);
        return true;
    };
    if (!expected || !expected->block || expected->block->isAir())
        return deny("expected air or unknown / 预期空气或未知");
    if (!mPlacements.viewState().layerRange.containsPosition(pos)) return deny("outside render layer / 超出渲染层");
    auto p    = mPlacements.findPlacement(expected->owner);
    auto cell = p ? placement::expectedAt(*p, pos) : std::nullopt;
    if (!cell) return deny("unknown target / 未知目标");
    auto        items = survival::resolveBlockItems(*p, *cell);
    auto const& held  = player.getSelectedItem();
    if (!items.pick || !held || survival::itemKey(held) != survival::itemKey(items.pick))
        return deny("wrong item / 手持物品错误");
    auto trace  = worldTrace(player);
    auto render = held.getBlockForRendering();
    if (!render || trace.mType != HitResultType::Tile || face >= 6)
        return deny("cannot predict placement / 无法预测放置");
    Vec3        click     = trace.mPos - Vec3(trace.mBlock);
    auto const& predicted = render->getBlockType().getPlacementBlock(player, pos, face, click, held.getAuxValue());
    if (!survival::placementOrientationMatches(*expected->block, predicted))
        return deny("orientation or state / 朝向或状态不符");
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
        finish("Placement unconfirmed / 放置未确认，已停止本次尝试");
        return;
    }
    auto p = mPlacements.findPlacement(mOwner);
    if (!p || !p->enabled || p->revision != mPlacementRevision
        || p->dimensionId != static_cast<int>(player.getDimensionId())) {
        finish("Target changed / 投影目标已改变");
        return;
    }
    auto expected = placement::expectedAt(*p, *mTarget);
    if (!expected || !expected->block || !mPlacements.viewState().layerRange.containsPosition(*mTarget)) {
        finish("Target outside layer / 目标超出渲染层");
        return;
    }
    auto& source = player.getDimension().getBlockSourceFromMainChunkSource();
    if (!source.hasChunksAt(*mTarget, 0, false)) {
        finish("Chunk unavailable / 区块未加载");
        return;
    }
    auto status = survival::compareBlocks(expected->block, &source.getBlock(*mTarget));
    if (mOperation.phase == survival::Phase::WaitingWorld) {
        if (status == verifier::VerificationStatus::Matched) {
            if (!mObservedMatch) {
                mObservedMatch = true;
                mSwapTime      = now;
            } else if (now - mSwapTime >= 250)
                finish("Placement observed; server validation separate / 已观察到匹配，服务端验收另计");
        } else if (mObservedMatch) finish("Placement corrected or rejected / 放置被纠正或拒绝");
        return;
    }
    if (status == verifier::VerificationStatus::Matched) {
        finish("Already matches / 目标已匹配");
        return;
    }
    if (status != verifier::VerificationStatus::MissingBlock) {
        finish("Clear or repair target manually / 请先手工清理或修正目标");
        return;
    }
    auto items = survival::resolveBlockItems(*p, *expected);
    if (!items.unsupported.empty() || !items.pick || items.rule.secondary) {
        finish(
            items.unsupported.empty() ? "Place multi-block item from its primary cell / 请从多格方块主格放置"
                                      : items.unsupported
        );
        return;
    }
    if (mOperation.phase == survival::Phase::Selecting) {
        auto result = survival::selectBuildItem(player, items.pick, cfg.hotbarSlots);
        if (result == survival::InventoryResult::Busy) return;
        if (result == survival::InventoryResult::Missing) {
            finish("Missing item / 缺少物品");
            return;
        }
        if (result == survival::InventoryResult::NoSlot || result == survival::InventoryResult::Unsupported) {
            finish("No usable hotbar slot / 无可用快捷栏槽位");
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
            finish("Inventory selection not confirmed / 换物未确认");
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
    if (configStore().snapshot().survival.materialHud && mMaterials.report().placementId == 0)
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
        } else mHud.push_back("No projection target / 未命中投影");
    }
    auto cfg = configStore().snapshot().survival;
    mTargetHudLines = mHud.size();
    if (cfg.materialHud) {
        auto report = mMaterials.report();
        mHud.push_back(report.building ? "Materials: scanning / 材料统计中" : "Materials / 材料（需准备/背包/总量）");
        size_t count = 0;
        for (auto const& row : report.rows)
            if (row.counts.remaining() && count++ < 5)
                mHud.push_back(
                    row.label + ": " + std::to_string(row.counts.toGather()) + " / "
                    + std::to_string(row.counts.available) + " / " + std::to_string(row.counts.total)
                );
    }
}
} 
