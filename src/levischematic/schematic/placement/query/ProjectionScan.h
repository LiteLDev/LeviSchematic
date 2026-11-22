#pragma once
#include "levischematic/schematic/placement/query/ProjectionQuery.h"
#include "levischematic/schematic/placement/store/ProjectionInstances.h"

namespace levischematic::placement {
struct ProjectionScan {
    std::vector<PlacementInstance>     instances;
    std::vector<size_t>                ends;
    std::vector<std::vector<BlockPos>> overrides;
    bool                               voidAsAir       = false;
    bool                               includeDisabled = false;
    void                               reset(PlacementState const&, PlacementId);
    size_t                             size() const { return ends.empty() ? 0 : ends.back(); }
    std::pair<PlacementInstance const*, std::optional<ExpectedPosition>> at(size_t) const;
};
} // namespace levischematic::placement
