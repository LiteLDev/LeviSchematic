#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace levischematic::selection::tool {
using Position = std::array<int, 3>;
using Vector   = std::array<double, 3>;
enum class Element { None, Corner1, Corner2, Box };
enum class CornerMode { Corners, Expand };
inline Position lookAxis(Vector const& direction, int amount) {
    size_t axis = 0;
    for (size_t i = 1; i < 3; ++i)
        if (std::abs(direction[i]) > std::abs(direction[axis])) axis = i;
    Position result{};
    result[axis] = direction[axis] < 0 ? -amount : amount;
    return result;
}
inline std::optional<Position> offset(Position pos, Position const& delta) {
    for (size_t i = 0; i < 3; ++i) {
        auto n = static_cast<long long>(pos[i]) + delta[i];
        if (n < std::numeric_limits<int>::min() || n > std::numeric_limits<int>::max()) return {};
        pos[i] = static_cast<int>(n);
    }
    return pos;
}
inline std::optional<double>
rayBox(Vector const& start, Vector const& direction, Position a, Position b, double limit = 200.) {
    if (!std::isfinite(limit) || limit < 0
        || direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2] < 1e-12)
        return {};
    double near = 0, far = limit;
    for (size_t i = 0; i < 3; ++i) {
        double lo = std::min(a[i], b[i]), hi = static_cast<double>(std::max(a[i], b[i])) + 1;
        if (!std::isfinite(start[i]) || !std::isfinite(direction[i])) return {};
        if (std::abs(direction[i]) < 1e-9) {
            if (start[i] < lo || start[i] > hi) return {};
        } else {
            double t1 = (lo - start[i]) / direction[i], t2 = (hi - start[i]) / direction[i];
            near = std::max(near, std::min(t1, t2));
            far  = std::min(far, std::max(t1, t2));
            if (far < near) return {};
        }
    }
    return near;
}
inline Element
selectElement(Vector const& start, Vector const& direction, std::optional<Position> a, std::optional<Position> b) {
    auto first  = a ? rayBox(start, direction, *a, *a) : std::nullopt;
    auto second = b ? rayBox(start, direction, *b, *b) : std::nullopt;
    if (first && (!second || *first <= *second)) return Element::Corner1;
    if (second) return Element::Corner2;
    return a && b && rayBox(start, direction, *a, *b) ? Element::Box : Element::None;
}
inline void expand(Position& a, Position& b, Position const& pos) {
    for (size_t i = 0; i < 3; ++i) {
        int  lo = std::min({a[i], b[i], pos[i]}), hi = std::max({a[i], b[i], pos[i]});
        bool reversed = a[i] > b[i];
        a[i]          = reversed ? hi : lo;
        b[i]          = reversed ? lo : hi;
    }
}
} 
