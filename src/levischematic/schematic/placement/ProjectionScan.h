#pragma once
#include "ProjectionInstances.h"
#include "ProjectionQuery.h"

namespace levischematic::placement {



struct ProjectionScan {
    std::vector<PlacementInstance>     instances;
    std::vector<size_t>                ends;
    std::vector<std::vector<BlockPos>> overrides;
    void                               reset(PlacementState const&, PlacementId);
    size_t                             size() const { return ends.empty() ? 0 : ends.back(); }
    std::pair<PlacementInstance const*, std::optional<ExpectedPosition>> at(size_t) const;
};
} 
