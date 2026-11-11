#include "PlayerInteraction.h"
#include "DataPick.h"
#include "PlacementInteraction.h"
#include "levischematic/survival/InventoryAccess.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/item/Item.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/ShapeType.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/phys/HitResult.h"
#include <cmath>

namespace levischematic::worldedit {
void PlayerInteraction::stop(LocalPlayer& player) {
    auto mode = player.mGameMode.get();
    if (mode && mDestroyAt) mode->stopDestroyBlock(*mDestroyAt);
    mDestroyAt.reset();
}
Outcome PlayerInteraction::acquireSourceItem(LocalPlayer& player, Target& target, uint64_t now) {
    if (!target.pickSource || (!target.sourceDataUnknown && !target.item.mUserData)) return Outcome::Observed;
    auto& source = player.getDimension().getBlockSourceFromMainChunkSource();
    auto  wait   = [&](std::string text) {
        detail = std::move(text);
        return pending() ? Outcome::InFlight : Outcome::Waiting;
    };
    if (pending() && now - mStarted > 3000) {
        reset();
        detail = "Original source item acquisition unconfirmed";
        return Outcome::Rejected;
    }
    if (mStage == Stage::Pick) {
        auto receipt      = DataPick::receipt();
        bool acknowledged = receipt.sequence > mPickTicket && receipt.slot >= 0 && receipt.slot < 9;
        if ((target.sourceDataUnknown && !acknowledged) || now - mStarted < 300)
            return wait("Waiting for original data-pick response / 等待原版带数据取物响应");
        auto const& picked = acknowledged ? player.getInventory().getItem(receipt.slot) : player.getSelectedItem();
        if (target.sourceDataUnknown
                ? (!picked || survival::itemKey(picked).name != survival::itemKey(target.item).name)
                : !survival::buildItemMatches(picked, target.item, survival::ItemMatch::Complete))
            return wait("Waiting for server data item / 等待服务端物品同步");
        target.item              = ItemStack(picked);
        target.sourceDataUnknown = false;
        mStage                   = Stage::Select;
    }
    if (mStage == Stage::Inventory) {
        if (!survival::buildItemMatches(player.getSelectedItem(), target.item, survival::ItemMatch::Complete)
            || now - mStarted < 300)
            return wait("Waiting for source item inventory update");
        mStage = Stage::Select;
    }
    if (mStage == Stage::Select && target.pickSource && (target.sourceDataUnknown || target.item.mUserData)) {
        auto selected =
            target.sourceDataUnknown
                ? survival::InventoryResult::Missing
                : survival::selectBuildItem(player, target.item, {0, 1, 2, 3, 4}, survival::ItemMatch::Complete, false);
        if (selected == survival::InventoryResult::Busy) return wait("Inventory busy");
        if (selected == survival::InventoryResult::Submitted) {
            mStage   = Stage::Inventory;
            mStarted = now;
            return Outcome::InFlight;
        }
        if (selected == survival::InventoryResult::NoSlot || selected == survival::InventoryResult::Unsupported) {
            detail = "No usable slot for source data item / 无可用槽位保存带数据物品";
            return Outcome::Unsupported;
        }
        if (selected == survival::InventoryResult::Missing) {
            if (!DataPick::available && target.sourceDataUnknown) {
                detail = "Original data-pick response API unavailable; remote source data cannot be confirmed";
                return Outcome::Unsupported;
            }
            auto at = *target.pickSource;
            if (!source.hasChunksAt(at, 0, false)) return wait("Waiting for source chunk");
            auto   eye = player.getHeadPos();
            Vec3   center{at.x + .5f, at.y + .5f, at.z + .5f};
            auto   delta = center - eye;
            double length =
                std::sqrt(double(delta.x) * delta.x + double(delta.y) * delta.y + double(delta.z) * delta.z);
            if (length > player.getPickRange() || length < .01)
                return wait("Move within source pick reach / 请靠近源方块取物");
            auto hit = source.clip(
                eye,
                center + delta * static_cast<float>(.1 / length),
                false,
                ShapeType::Outline,
                16,
                false,
                false,
                &player,
                [](BlockSource const&, Block const&, bool) { return true; },
                false
            );
            if (hit.mType != HitResultType::Tile || hit.mBlock != at)
                return wait("Source pick line of sight blocked / 源方块取物视线受阻");
            mPickTicket = DataPick::receipt().sequence;
            mStage      = Stage::Pick;
            mStarted    = now;
            player.pickBlock(hit, true);
            return Outcome::InFlight;
        }
    }
    return Outcome::Observed;
}
Outcome PlayerInteraction::advance(LocalPlayer& player, Target& target, uint64_t now) {
    auto&      source = player.getDimension().getBlockSourceFromMainChunkSource();
    auto const pos    = target.position;
    auto       wait   = [&](std::string why) {
        detail = std::move(why);
        return pending() ? Outcome::InFlight : Outcome::Waiting;
    };
    if (!source.hasChunksAt(pos, 0, false)) return wait("Chunk unavailable / 等待区块加载");
    if (pending() && now - mStarted > 3000) {
        auto stage = mStage;
        stop(player);
        reset();
        detail = stage == Stage::Pick      ? "Server data pick unconfirmed / 服务端带数据取物未确认"
               : stage == Stage::Inventory ? "Inventory item unconfirmed / 背包物品未确认"
                                           : "World change unconfirmed / 世界修改未确认";
        return Outcome::Rejected;
    }
    if (target.pickSource && mStage != Stage::World) {
        auto acquisition = acquireSourceItem(player, target, now);
        if (acquisition != Outcome::Observed) return acquisition;
    }
    
    
    bool matches = mStage == Stage::World ? survival::compareBlocks(target.block, &source.getBlock(pos))
                                                == verifier::VerificationStatus::Matched
                                          : blockMatches(source, pos, target.block, target.item);
    if (mStage == Stage::World) {
        
        if (mDestroy && source.getBlock(pos).isAir()) {
            stop(player);
            if (target.block->isAir()) matches = true;
            else {
                reset();
                return Outcome::Waiting;
            }
        }
        if (!matches) {
            mMatched = 0;
            return wait("Waiting for world update / 等待世界更新");
        }
        if (!mMatched) mMatched = now;
        if (now - mMatched < 350) return wait("Observing world result / 等待结果稳定");
        reset();
        return Outcome::Observed;
    }
    if (matches) {
        reset();
        return Outcome::Observed;
    }
    auto  eye = player.getHeadPos(), direction = player.getViewVector(1);
    float reach = player.getPickRange();
    Vec3  center{pos.x + .5f, pos.y + .5f, pos.z + .5f};
    auto  dx = center.x - eye.x, dy = center.y - eye.y, dz = center.z - eye.z;
    if (dx * dx + dy * dy + dz * dz > (reach + .8f) * (reach + .8f)) return wait("Move within reach / 请靠近目标");
    auto hit = source.clip(
        eye,
        eye + direction * reach,
        false,
        ShapeType::Outline,
        static_cast<int>(std::ceil(reach)),
        false,
        false,
        &player,
        [](BlockSource const&, Block const&, bool) { return true; },
        false
    );
    if (hit.mType != HitResultType::Tile || hit.mFacing >= 6)
        return wait("Aim at target/support face / 请瞄准目标或支撑面");
    auto mode = player.mGameMode.get();
    if (!mode) return wait("Game mode unavailable");
    
    if (!target.block->isAir()) {
        if (!target.item) {
            detail = "No placement item";
            return Outcome::Unsupported;
        }
        if (mStage == Stage::Inventory) {
            if (!survival::buildItemMatches(player.getSelectedItem(), target.item, survival::ItemMatch::Complete))
                return wait("Waiting for data item / 等待带数据物品");
            if (now - mStarted < 300) return wait("Waiting for inventory update / 等待背包更新");
            mStage = Stage::Select;
        }
        auto selected = survival::selectBuildItem(player, target.item, {0, 1, 2, 3, 4}, survival::ItemMatch::Complete);
        if (selected == survival::InventoryResult::Busy) return wait("Inventory busy");
        if (selected == survival::InventoryResult::Submitted) {
            mStage   = Stage::Inventory;
            mStarted = now;
            return Outcome::InFlight;
        }
        if (selected != survival::InventoryResult::Ready) {
            detail = "Cannot select build item";
            return Outcome::Unsupported;
        }
    }
    if (!source.getBlock(pos).isAir()) {
        if (hit.mBlock != pos) return wait("Aim at block to remove / 请瞄准待清理方块");
        bool destroyed = false;
        if (!mode->startDestroyBlock(pos, hit.mFacing, destroyed) && !destroyed) {
            detail = "Vanilla destruction declined";
            return Outcome::Rejected;
        }
        mDestroy   = true;
        mDestroyAt = pos;
        mStage     = Stage::World;
        mStarted   = now;
        return Outcome::InFlight;
    }
    if (target.block->isAir()) return Outcome::Observed;
    std::string reason;
    if (!buildAtAim(player, pos, *target.block, reason)) return wait(std::move(reason));
    mDestroy = false;
    mStage   = Stage::World;
    mStarted = now;
    mMatched = 0;
    return Outcome::InFlight;
}
} 
