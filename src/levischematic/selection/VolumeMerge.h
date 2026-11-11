#pragma once
#include "levischematic/schematic/placement/VolumeIndex.h"
#include <span>
#include <stdexcept>

namespace levischematic::selection {

inline void mergeBlockLayer(
    std::span<int>       output,
    std::span<int const> input,
    std::span<int const> palette,
    std::array<int, 3>   tileSize,
    std::array<int, 3>   size,
    std::array<int, 3>   offset
) {
    auto volume = placement::volumeSize(size), tileVolume = placement::volumeSize(tileSize);
    if (!volume || !tileVolume || output.size() != *volume || input.size() != *tileVolume)
        throw std::runtime_error("Invalid capture layer volume.");
    for (size_t axis = 0; axis < 3; ++axis)
        if (offset[axis] < 0 || int64_t{offset[axis]} + tileSize[axis] > size[axis])
            throw std::runtime_error("Capture tile is outside selection.");
    for (int index : input)
        if (index < -1 || (index >= 0 && static_cast<size_t>(index) >= palette.size()))
            throw std::runtime_error("Invalid capture palette index.");
    for (size_t i = 0; i < input.size(); ++i) {
        auto pos = placement::volumePosition(i, tileSize);
        for (size_t axis = 0; axis < 3; ++axis) pos[axis] += offset[axis];
        output[placement::volumeIndex(pos, size)] = input[i] < 0 ? -1 : palette[input[i]];
    }
}
} 
