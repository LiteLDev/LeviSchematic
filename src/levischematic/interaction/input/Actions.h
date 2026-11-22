#pragma once
#include <array>
#include <string_view>
#include <vector>

namespace levischematic::input {
enum class ActionId {
    MainMenu,
    Settings,
    Placements,
    PlacementSettings,
    SelectionSettings,
    SelectionManager,
    ToggleTool,
    CycleMode,
    CycleModeReverse,
    CycleSubmode,
    Primary,
    Secondary,
    Select,
    NudgePositive,
    NudgeNegative,
    IncreaseLayer,
    DecreaseLayer,
    NextLayerMode,
    PreviousLayerMode,
    SetLayerHere,
    SaveSelection,
    Verifier,
    Materials,
    PickBlock,
    TogglePickBlock,
    Info,
    InfoRelease,
    EasyPlace,
    EasyPlaceRelease,
    ToggleEasyPlace,
    ToggleRestriction,
    ExecuteOperation,
    WorldOperations,
    Projects,
    SignCopy,
    SignCopyBack,
    SignPaste,
    SampleSecondary,
};
enum class Phase { Press, Release };
inline constexpr int WheelUp = -1001, WheelDown = -1002;
struct ActionSpec {
    ActionId         id;
    std::string_view name;
    std::string_view label;
    std::vector<int> keys;
    Phase            phase       = Phase::Press;
    bool             tool        = false;
    bool             sdkCallback = false;
};
inline auto const& actionSpecs() {
    static const std::array specs{
        ActionSpec{ActionId::ExecuteOperation, "levischematic.world.execute", "key.world.execute", {}},
        ActionSpec{ActionId::WorldOperations, "levischematic.ui.world", "key.ui.world", {}},
        ActionSpec{ActionId::Projects, "levischematic.ui.projects", "key.ui.projects", {}},
        ActionSpec{ActionId::SignCopy, "levischematic.sign.copy", "key.sign.copy", {}},
        ActionSpec{ActionId::SignCopyBack, "levischematic.sign.copy_back", "key.sign.copy_back", {}},
        ActionSpec{ActionId::SignPaste, "levischematic.sign.paste", "key.sign.paste", {}},
        ActionSpec{ActionId::SampleSecondary, "levischematic.world.sample_secondary", "key.world.sample_secondary", {}},
        ActionSpec{ActionId::Materials, "levischematic.ui.materials", "key.ui.materials", {77, 76}},
        ActionSpec{
                   ActionId::SelectionManager,
                   "levischematic.ui.selection_manager", "key.ui.selection_manager",
                   {77, 83}
        },
        ActionSpec{ActionId::PickBlock, "levischematic.survival.pick", "key.survival.pick", {-3}},
        ActionSpec{
                   ActionId::TogglePickBlock,
                   "levischematic.survival.pick.toggle", "key.survival.pick.toggle",
                   {77, -3}
        },
        ActionSpec{ActionId::Info, "levischematic.survival.info", "key.survival.info", {73}},
        ActionSpec{
                   ActionId::InfoRelease,
                   "levischematic.survival.info.release", "key.survival.info.release",
                   {73},
                   Phase::Release
        },
        ActionSpec{ActionId::EasyPlace, "levischematic.survival.use", "key.survival.use", {-2}},
        ActionSpec{
                   ActionId::EasyPlaceRelease,
                   "levischematic.survival.use.release", "key.survival.use.release",
                   {-2},
                   Phase::Release
        },
        ActionSpec{ActionId::ToggleEasyPlace, "levischematic.survival.easy.toggle", "key.survival.easy.toggle", {}},
        ActionSpec{
                   ActionId::ToggleRestriction,
                   "levischematic.survival.restrict.toggle", "key.survival.restrict.toggle",
                   {}
        },
        ActionSpec{ActionId::NextLayerMode, "levischematic.view.mode.next", "key.view.mode.next", {77, 33}},
        ActionSpec{ActionId::PreviousLayerMode, "levischematic.view.mode.previous", "key.view.mode.previous", {77, 34}},
        ActionSpec{ActionId::SetLayerHere, "levischematic.view.here", "key.view.here", {}},
        ActionSpec{ActionId::SaveSelection, "levischematic.selection.save", "key.selection.save", {17, 18, 83}},
        ActionSpec{ActionId::Verifier, "levischematic.ui.verifier", "key.ui.verifier", {77, 86}},
        ActionSpec{ActionId::MainMenu, "levischematic.ui.open", "key.ui.open", {77}, Phase::Release},
        ActionSpec{ActionId::Settings, "levischematic.ui.settings", "key.ui.settings", {77, 67}},
        ActionSpec{ActionId::Placements, "levischematic.ui.placements", "key.ui.placements", {77, 80}},
        ActionSpec{
                   ActionId::PlacementSettings,
                   "levischematic.ui.placement", "key.ui.placement",
                   {109},
                   Phase::Press,
                   false, true
        },
        ActionSpec{
                   ActionId::SelectionSettings,
                   "levischematic.ui.selection", "key.ui.selection",
                   {106},
                   Phase::Press,
                   false, true
        },
        ActionSpec{ActionId::ToggleTool, "levischematic.tool.toggle", "key.tool.toggle", {77, 84}},
        ActionSpec{ActionId::CycleMode, "levischematic.tool.mode", "key.tool.mode", {17, WheelUp}, Phase::Press, true},
        ActionSpec{
                   ActionId::CycleModeReverse,
                   "levischematic.tool.mode.reverse", "key.tool.mode.reverse",
                   {17, WheelDown},
                   Phase::Press,
                   true
        },
        ActionSpec{ActionId::CycleSubmode, "levischematic.tool.submode", "key.tool.submode", {17, 77}},
        ActionSpec{ActionId::Primary, "levischematic.tool.primary", "key.tool.primary", {-1}, Phase::Press, true},
        ActionSpec{ActionId::Secondary, "levischematic.tool.secondary", "key.tool.secondary", {-2}, Phase::Press, true},
        ActionSpec{ActionId::Select, "levischematic.tool.select", "key.tool.select", {-3}, Phase::Press, true},
        ActionSpec{
                   ActionId::NudgePositive,
                   "levischematic.tool.nudge.positive", "key.tool.nudge.positive",
                   {18, WheelUp},
                   Phase::Press,
                   true
        },
        ActionSpec{
                   ActionId::NudgeNegative,
                   "levischematic.tool.nudge.negative", "key.tool.nudge.negative",
                   {18, WheelDown},
                   Phase::Press,
                   true
        },
        ActionSpec{
                   ActionId::IncreaseLayer,
                   "levischematic.view.y.max.increment", "key.view.y.max.increment",
                   {33},
                   Phase::Press,
                   false, true
        },
        ActionSpec{
                   ActionId::DecreaseLayer,
                   "levischematic.view.y.max.decrement", "key.view.y.max.decrement",
                   {34},
                   Phase::Press,
                   false, true
        },
    };
    return specs;
}
inline ActionSpec const& actionSpec(ActionId id) {
    for (auto const& spec : actionSpecs())
        if (spec.id == id) return spec;
    return actionSpecs().front();
}
} // namespace levischematic::input
