#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace levischematic::render::detail {

constexpr std::optional<std::array<float, 2>> vanillaDisplayMinecartSize(std::string_view identifier) {
    if (identifier == "minecraft:minecart" || identifier == "minecraft:chest_minecart"
        || identifier == "minecraft:hopper_minecart" || identifier == "minecraft:tnt_minecart"
        || identifier == "minecraft:command_block_minecart")
        return std::array<float, 2>{0.98f, 0.7f};
    return std::nullopt;
}

} // namespace levischematic::render::detail
