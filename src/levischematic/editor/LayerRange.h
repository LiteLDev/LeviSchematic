#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace levischematic::editor {

enum class LayerAxis { X, Y, Z };
enum class LayerMode { All, Single, Range, Below, Above };


struct LayerRange {
    int       minY = -64, maxY = 320;
    bool      enabled = false;
    LayerAxis axis    = LayerAxis::Y;
    LayerMode mode    = LayerMode::All;
    int       layer   = 0;
    int       above = 0, below = 0;
    bool      followPlayer = false;
    bool      moveMinimum = false, moveMaximum = false;

    [[nodiscard]] int boundary() const {
        return mode == LayerMode::Above ? above : mode == LayerMode::Below ? below : layer;
    }
    int& boundary() { return mode == LayerMode::Above ? above : mode == LayerMode::Below ? below : layer; }

    [[nodiscard]] bool contains(int coordinate) const noexcept {
        if (!enabled) return true;
        switch (mode) {
        case LayerMode::All: 
        case LayerMode::Range:
            return coordinate >= minY && coordinate <= maxY;
        case LayerMode::Single:
            return coordinate == layer;
        case LayerMode::Below:
            return coordinate <= below;
        case LayerMode::Above:
            return coordinate >= above;
        }
        return false;
    }
    template <class Position>
    [[nodiscard]] bool containsPosition(Position const& pos) const noexcept {
        return contains(axis == LayerAxis::X ? pos.x : axis == LayerAxis::Y ? pos.y : pos.z);
    }
    bool operator==(LayerRange const&) const = default;
};


inline bool stepLayer(LayerRange& range, int delta, double playerCoordinate = 0) {
    if (!range.enabled || delta == 0) return false;
    auto previous = range;
    auto clamp    = [](int64_t value) {
        return static_cast<int>(std::clamp(value, int64_t{INT32_MIN}, int64_t{INT32_MAX}));
    };
    if (range.mode == LayerMode::Range || range.mode == LayerMode::All) {
        bool nearestMin = playerCoordinate < double(range.minY) + 0.5
                       || std::abs(playerCoordinate - (double(range.minY) + 0.5))
                              < std::abs(playerCoordinate - (double(range.maxY) + 0.5));
        bool moveMin    = range.moveMinimum || (nearestMin && !range.moveMaximum);
        bool moveMax    = range.moveMaximum || (!nearestMin && !range.moveMinimum);
        if (moveMin && moveMax) {
            auto shift = std::clamp(int64_t{delta}, int64_t{INT32_MIN} - range.minY, int64_t{INT32_MAX} - range.maxY);
            range.minY = clamp(int64_t{range.minY} + shift);
            range.maxY = clamp(int64_t{range.maxY} + shift);
        } else if (moveMin) range.minY = std::min(clamp(int64_t{range.minY} + delta), range.maxY);
        else if (moveMax) range.maxY = std::max(clamp(int64_t{range.maxY} + delta), range.minY);
    } else range.boundary() = clamp(int64_t{range.boundary()} + delta);
    return range != previous;
}

} 
