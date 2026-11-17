#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace levischematic::render::detail {

constexpr bool useWorldItemForLegacyMesh(
    std::string_view            source,
    std::size_t                 textureCount,
    bool                        movingBlock,
    bool                        ui,
    bool                        firstPerson,
    std::array<float, 4> const& color
) {
    return (source == "entity" || source == "entity_static" || source == "entity_alphatest") && textureCount == 1
        && movingBlock && !ui && !firstPerson && color == std::array<float, 4>{1, 1, 1, 1};
}

}
