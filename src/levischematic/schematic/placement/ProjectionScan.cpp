#include "ProjectionScan.h"
#include "levischematic/util/PositionUtils.h"
#include <algorithm>

namespace levischematic::placement {
void ProjectionScan::reset(PlacementState const& state, PlacementId id) {
    ensureProjectionInstances(state);
    instances.clear();
    ends.clear();
    overrides.clear();
    size_t count = 0;
    
    
    for (auto it = state.projectionOrder.rbegin(); it != state.projectionOrder.rend(); ++it) {
        auto const& p = state.projections.at(*it);
        if ((p.parentId ? p.parentId : p.id) != id || !p.enabled || !p.asset) continue;
        instances.push_back(p);
        overrides.emplace_back();
        for (auto const& [key, value] : p.overrides) {
            auto at = toWorld(p, util::decodePosKey(key));
            if (at) overrides.back().push_back(*at);
        }
        count += p.asset->indices.size() + overrides.back().size();
        ends.push_back(count);
    }
}
std::pair<PlacementInstance const*, std::optional<ExpectedPosition>> ProjectionScan::at(size_t cursor) const {
    auto it = std::upper_bound(ends.begin(), ends.end(), cursor);
    if (it == ends.end()) return {};
    auto        i      = static_cast<size_t>(it - ends.begin());
    auto        offset = cursor - (i ? ends[i - 1] : 0);
    auto const& p      = instances[i];
    auto        volume = p.asset->indices.size();
    return {&p, offset < volume ? expectedPosition(p, offset) : expectedAt(p, overrides[i][offset - volume])};
}
} 
