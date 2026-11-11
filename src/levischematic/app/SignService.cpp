#include "SignService.h"
#include "PlacementService.h"
#include "levischematic/worldedit/BlockData.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/StringTag.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/ShapeType.h"
#include "mc/world/level/block/actor/SignBlockActor.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/phys/HitResult.h"
namespace levischematic::app {
std::string SignService::copy(IClientInstance& client, PlacementService& placements, bool back) {
    auto player = client.getLocalPlayer();
    if (!player) return mDetail = "Player unavailable.";
    auto  start = player->getHeadPos(), dir = player->getViewVector(1);
    auto  side   = back ? SignTextSide::Back : SignTextSide::Front;
    auto& dim    = player->getDimension();
    auto& source = dim.getBlockSourceFromMainChunkSource();
    try {
        auto hit = source.clip(
            start,
            start + dir * player->getPickRange(),
            false,
            ShapeType::Outline,
            16,
            false,
            false,
            player,
            [](BlockSource const&, Block const&, bool) { return true; },
            false
        );
        auto projected = placement::traceProjection(
            placements.orderedPlacements(),
            static_cast<int>(player->getDimensionId()),
            placements.viewState().layerRange,
            {start.x, start.y, start.z},
            {dir.x, dir.y, dir.z},
            player->getPickRange()
        );
        if (projected) {
            auto p        = placements.findPlacement(projected->owner);
            auto expected = p ? placement::expectedAt(*p, projected->position) : std::nullopt;
            if (p && expected) {
                auto item = worldedit::projectionBlockItem(*p, *expected, dim);
                if (item.mUserData) {
                    auto name = back ? "BackText" : "FrontText";
                    if (item.mUserData->contains(name) && item.mUserData->at(name).hold<CompoundTag>()) {
                        auto const& text = item.mUserData->at(name).get<CompoundTag>();
                        if (text.contains("Text") && text.at("Text").hold<StringTag>()) {
                            mText          = text.at("Text").get<StringTag>();
                            mHasText       = true;
                            return mDetail = "Projection sign text copied.";
                        }
                    }
                }
            }
        }
        if (hit.mType == HitResultType::Tile) {
            auto actor = source.getBlockEntity(hit.mBlock);
            if (actor && actor->isTypeOrDerived(BlockActorType::Sign)) {
                mText          = static_cast<SignBlockActor*>(actor)->getMessage(side);
                mHasText       = true;
                return mDetail = "Sign text copied.";
            }
        }
        return mDetail = "Aim at a real or projected sign / 请瞄准告示牌";
    } catch (std::exception const& e) {
        return mDetail = e.what();
    }
}
std::string SignService::arm() {
    if (!editorAvailable) return mDetail = "Native sign editor entry unavailable / 原版告示牌编辑入口不可用";
    if (!mHasText) return mDetail = "Copy sign text first / 请先复制告示牌文本";
    mArmed         = true;
    return mDetail = "Paste armed: normally open the desired sign side / 请正常打开目标告示牌";
}
std::optional<std::string> SignService::consume() {
    if (!mArmed) return {};
    mArmed  = false;
    mDetail = "Text supplied to vanilla sign editor; server acceptance separate.";
    return mText;
}
} 
