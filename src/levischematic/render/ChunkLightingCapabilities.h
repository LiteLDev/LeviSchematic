#pragma once

#include "mc/deps/minecraft_renderer/framebuilder/FrameLightingModelCapabilities.h"

#include <array>
#include <cstddef>

namespace levischematic::render::detail {


struct ChunkLightingCapabilities {
    alignas(4) mce::framebuilder::FrameLightingModelCapabilities value{};
    std::array<std::byte, 7> padding{};
};

} 
