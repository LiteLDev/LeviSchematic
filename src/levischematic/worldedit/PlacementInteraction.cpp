#include "PlacementInteraction.h"
#include "levischematic/survival/BlockItems.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/item/Item.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/ShapeType.h"
#include "mc/world/level/dimension/Dimension.h"
#include <cmath>
namespace levischematic::worldedit {
HitResult traceWorld(LocalPlayer& player) {
    auto start = player.getHeadPos(), dir = player.getViewVector(1);
    auto reach = player.getPickRange();
    return player.getDimension().getBlockSourceFromMainChunkSource().clip(
        start,
        start + dir * reach,
        false,
        ShapeType::Outline,
        static_cast<int>(std::ceil(reach)),
        false,
        false,
        &player,
        [](BlockSource const&, Block const&, bool) { return true; },
        false
    );
}
bool buildAtAim(LocalPlayer& player, BlockPos const& target, Block const& expected, std::string& reason) {
    auto deny = [&](char const* text) {
        reason = text;
        return false;
    };
    auto hit = traceWorld(player);
    if (hit.mType != HitResultType::Tile || hit.mFacing >= 6) return deny("Aim at a real support face / 请瞄准支撑面");
    auto const& held = player.getSelectedItem();
    auto        item = held.mItem.get();
    auto        at   = hit.mBlock;
    auto        face = hit.mFacing;
    if (!item || !item->_calculatePlacePos(held, player, face, at) || at != target)
        return deny("No valid support / 请瞄准目标的有效支撑面");
    auto render = held.getBlockForRendering();
    if (!render) return deny("Placement prediction unavailable");
    auto const& predicted =
        render->getBlockType().getPlacementBlock(player, at, face, hit.mPos - Vec3(hit.mBlock), held.getAuxValue());
    if (survival::compareBlocks(&expected, &predicted) != verifier::VerificationStatus::Matched)
        return deny("Adjust orientation/state / 请调整朝向或状态");
    auto mode = player.mGameMode.get();
    if (!mode || !mode->buildBlock(hit.mBlock, hit.mFacing, HandSlot::Mainhand, false))
        return deny("Vanilla placement declined / 原版拒绝放置");
    return true;
}
} 
