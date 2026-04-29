#include "InfoOverlayService.h"

#include "levischematic/app/PlacementService.h"
#include "levischematic/app/SelectionService.h"

#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/item/VanillaItemNames.h"

namespace levischematic::app {

namespace {

[[nodiscard]] std::string formatBlockPos(BlockPos const& pos) {
    return "(" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + ")";
}

[[nodiscard]] std::string formatSize(BlockPos const& size) {
    return std::to_string(size.x) + "x" + std::to_string(size.y) + "x" + std::to_string(size.z);
}

} // namespace

std::optional<InfoOverlayView> InfoOverlayService::buildView(LocalPlayer& player) const {
    if (!isHoldingStick(player)) {
        return std::nullopt;
    }

    auto overlay = mSelectionService.overlay();
    if (mSelectionService.isSelectionModeEnabled()) {
        return InfoOverlayView{
            {
             formatSizeLine(overlay.size),
             formatOriginLine(overlay.origin),
             "模式: 保存投影模式",
             }
        };
    }

    auto placementResult = mPlacementService.requireSelectedPlacement();
    if (!placementResult) {
        return std::nullopt;
    }

    auto const* placement = placementResult.value();
    if (!placement) {
        return std::nullopt;
    }

    return InfoOverlayView{
        {
         formatSizeLine(placement->asset ? std::optional<BlockPos>(placement->asset->size) : std::nullopt),
         formatOriginLine(placement->origin),
         "模式: 放置投影模式",
         }
    };
}

bool InfoOverlayService::isHoldingStick(LocalPlayer& player) const {
    if (!player.mInventory || !player.mInventory->mInventory) {
        return false;
    }

    auto& item = player.mInventory->mInventory->getItem(player.mInventory->mSelected);
    return item.isInstance(VanillaItemNames::Stick(), false);
}

std::string InfoOverlayService::formatOriginLine(std::optional<BlockPos> const& pos) const {
    return "原点: " + (pos ? pos->toString() : "未设置");
}

std::string InfoOverlayService::formatSizeLine(std::optional<BlockPos> const& size) const {
    return "大小: " + (size ? size->toString() : "未设置");
}

} // namespace levischematic::app
