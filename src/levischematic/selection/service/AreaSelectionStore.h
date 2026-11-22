#pragma once
#include "levischematic/selection/model/SelectionState.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace levischematic::selection {
inline constexpr int AreaSelectionFormat = 1;

nlohmann::json encodeAreaSelection(SelectionState const& state);
SelectionState decodeAreaSelection(nlohmann::json const& value, SelectionState base);

std::string saveAreaSelection(std::filesystem::path const& file, SelectionState const& state);
std::optional<SelectionState>
loadAreaSelection(std::filesystem::path const& file, SelectionState const& base, std::string& error);
std::string describeAreaSelection(std::filesystem::path const& file);
} // namespace levischematic::selection
