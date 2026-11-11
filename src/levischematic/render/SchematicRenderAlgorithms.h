#pragma once






#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace levischematic::render::detail {

using Point      = std::array<float, 3>;
using GridOrigin = std::array<int, 3>;

inline double distanceSquaredToBox(Point const& camera, Point const& lower, Point const& upper) {
    double distance = 0;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(camera[axis]) || !std::isfinite(lower[axis]) || !std::isfinite(upper[axis])
            || lower[axis] > upper[axis])
            return 0;
        double delta = std::max(
            {static_cast<double>(lower[axis]) - camera[axis], static_cast<double>(camera[axis]) - upper[axis], 0.0}
        );
        distance += delta * delta;
    }
    return distance;
}

inline bool withinRenderDistance(Point const& camera, Point const& lower, Point const& upper, int distance) {
    return distance <= 0 || distanceSquaredToBox(camera, lower, upper) <= static_cast<double>(distance) * distance;
}

template <class Candidate>
void prioritizeVisibleBuilds(std::vector<Candidate>& candidates, std::size_t rotation) {
    if (candidates.empty()) return;
    std::rotate(candidates.begin(), candidates.begin() + rotation % candidates.size(), candidates.end());
    std::stable_sort(candidates.begin(), candidates.end(), [](auto const& first, auto const& second) {
        return first.priority < second.priority;
    });
}







inline bool validArea(Point const& lower, Point const& upper) {
    constexpr double limit = static_cast<double>(std::numeric_limits<int>::max()) - 16;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        double lo = lower[axis], hi = upper[axis];
        if (!std::isfinite(lo) || !std::isfinite(hi) || hi <= lo || lo < std::numeric_limits<int>::min()
            || hi > limit) {
            return false;
        }
    }
    return true;
}









inline std::vector<GridOrigin> generateChunkOrigins(Point const& lower, Point const& upper) {
    std::vector<GridOrigin> result;
    if (!validArea(lower, upper)) {
        return result;
    }

    auto start =
    



    [](float value) { return static_cast<int>(std::floor(static_cast<double>(value) / 16)) * 16; };

    for (int x = start(lower[0]); x < static_cast<double>(upper[0]); x += 16) {
        for (int y = start(lower[1]); y < static_cast<double>(upper[1]); y += 16) {
            for (int z = start(lower[2]); z < static_cast<double>(upper[2]); z += 16) {
                result.push_back({x, y, z});
            }
        }
    }
    return result;
}








[[nodiscard]] inline GridOrigin rotatedStructureSize(GridOrigin const& size, bool swapHorizontalAxes) {
    return swapHorizontalAxes ? GridOrigin{size[2], size[1], size[0]} : size;
}









inline bool withinBlockEntityBounds(Point const& position, Point const& lower, Point const& upper) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (!(position[axis] >= lower[axis] - 0.010f && position[axis] <= upper[axis])) {
            return false;
        }
    }
    return true;
}

} 
