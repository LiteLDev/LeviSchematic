#pragma once
#include "levischematic/presentation/ui/model/Ui.h"
#include "mc/world/level/BlockPos.h"
#include <filesystem>
#include <optional>
#include <string>

namespace levischematic::ui {
inline constexpr char const* PlusMinusTip      = "ui.tip.plus_minus";
inline constexpr char const* PlusMinusTipCtrl  = "ui.tip.plus_minus_ctrl";
inline constexpr char const* LockCoordinateTip = "ui.tip.lock_coordinate";

bool                    parseInt(std::string const& text, int& value);
std::optional<BlockPos> playerBlockPos();
int                     playerDimension();
std::string             utf8Path(std::filesystem::path const& path);
std::filesystem::path   pathFromUtf8(std::string const& text);
void                    mainMenuButton(PageBuilder& b);
void navButton(PageBuilder& b, std::string id, std::string label, PageId target, Slot slot, IconRef icon = {});
void confirm(std::shared_ptr<Page> const& p, std::string message, std::function<void()> action);
void prompt(
    std::shared_ptr<Page> const&     p,
    std::string                      message,
    std::string                      initial,
    std::function<void(std::string)> action
);
std::string formatTime(int64_t milliseconds);
} // namespace levischematic::ui
