#pragma once
#include "ToolGeometry.h"
#include <array>
#include <optional>

namespace levischematic::selection {


struct GrowSelection {
    tool::Position      minimum{}, maximum{};
    int                 face   = 0;
    int64_t             cursor = 0;
    std::array<bool, 6> expand{};
    bool                done = false, failed = false;
    size_t              scanned = 0;
    template <class Sample>
    void step(Sample&& sample) {
        if (done || failed) return;
        int     axis = face / 2, a = (axis + 1) % 3, b = (axis + 2) % 3;
        int64_t width  = int64_t(maximum[a]) - minimum[a] + 1;
        int64_t height = int64_t(maximum[b]) - minimum[b] + 1;
        if (width <= 0 || height <= 0 || width > INT32_MAX || height > INT32_MAX) {
            failed = true;
            return;
        }
        std::array<int64_t, 3> point{minimum[0], minimum[1], minimum[2]};
        point[axis]  = face % 2 ? int64_t(maximum[axis]) + 1 : int64_t(minimum[axis]) - 1;
        point[a]    += cursor / height;
        point[b]    += cursor % height;
        for (auto n : point)
            if (n < INT32_MIN || n > INT32_MAX) {
                failed = true;
                return;
            }
        auto solid =
            sample(tool::Position{static_cast<int>(point[0]), static_cast<int>(point[1]), static_cast<int>(point[2])});
        ++scanned;
        if (!solid) {
            failed = true;
            return;
        }
        if (*solid) expand[face] = true;
        if (*solid || ++cursor == width * height) {
            ++face;
            cursor = 0;
        }
        if (face != 6) return;
        bool changed = false;
        for (int i = 0; i < 3; ++i) {
            if (expand[i * 2]) {
                --minimum[i];
                changed = true;
            }
            if (expand[i * 2 + 1]) {
                ++maximum[i];
                changed = true;
            }
            if (int64_t(maximum[i]) - minimum[i] + 1 > INT32_MAX) {
                failed = true;
                return;
            }
        }
        done   = !changed;
        face   = 0;
        expand = {};
    }
};
} 
