#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
namespace levischematic::util {

template <class Visit>
void traceVoxels(std::array<double, 3> start, std::array<double, 3> direction, double reach, Visit visit) {
    double length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
    if (!std::isfinite(length) || length < 1e-9 || !std::isfinite(reach) || reach < 0 || reach > 256) return;
    std::array<int, 3>    pos{}, step{};
    std::array<double, 3> next{}, delta{};
    for (size_t a = 0; a < 3; ++a) {
        if (!std::isfinite(start[a]) || start[a] < INT32_MIN + 1.0 || start[a] > INT32_MAX - 1.0) return;
        direction[a] /= length;
        pos[a]        = static_cast<int>(std::floor(start[a]));
        step[a]       = direction[a] < 0 ? -1 : 1;
        delta[a] =
            std::abs(direction[a]) < 1e-12 ? std::numeric_limits<double>::infinity() : std::abs(1 / direction[a]);
        next[a] = std::isinf(delta[a]) ? delta[a]
                                       : (direction[a] < 0 ? start[a] - pos[a] : pos[a] + 1.0 - start[a]) * delta[a];
    }
    double distance = 0;
    while (distance <= reach) {
        if (!visit(pos, distance)) return;
        distance = std::min({next[0], next[1], next[2]});
        if (!std::isfinite(distance)) return;
        for (size_t a = 0; a < 3; ++a)
            if (next[a] <= distance + 1e-10) {
                if ((step[a] > 0 && pos[a] == INT32_MAX) || (step[a] < 0 && pos[a] == INT32_MIN)) return;
                pos[a]  += step[a];
                next[a] += delta[a];
            }
    }
}
} 
