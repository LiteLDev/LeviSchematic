#pragma once

#include "mc/world/level/BlockPos.h"

#include <optional>
#include <string>
#include <vector>

class LocalPlayer;

namespace levischematic::app {

class PlacementService;
class SelectionService;

struct InfoOverlayView {
    std::vector<std::string> lines;
};

class InfoOverlayService {
public:
    InfoOverlayService(PlacementService& placementService, SelectionService& selectionService)
        : mPlacementService(placementService)
        , mSelectionService(selectionService) {}

    [[nodiscard]] std::optional<InfoOverlayView> buildView(LocalPlayer& player) const;

private:
    [[nodiscard]] bool        isHoldingStick(LocalPlayer& player) const;
    [[nodiscard]] std::string formatOriginLine(std::optional<BlockPos> const& pos) const;
    [[nodiscard]] std::string formatSizeLine(std::optional<BlockPos> const& size) const;

    PlacementService& mPlacementService;
    SelectionService& mSelectionService;
};

} // namespace levischematic::app
