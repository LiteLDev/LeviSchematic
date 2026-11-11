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
        ActionSpec{ActionId::ExecuteOperation, "levischematic.world.execute", "Execute operation / 执行创造操作", {}},
        ActionSpec{ActionId::WorldOperations, "levischematic.ui.world", "World operations / 创造工具", {}},
        ActionSpec{ActionId::Projects, "levischematic.ui.projects", "Projects / 建筑项目", {}},
        ActionSpec{ActionId::SignCopy, "levischematic.sign.copy", "Copy sign front / 复制告示牌正面", {}},
        ActionSpec{ActionId::SignCopyBack, "levischematic.sign.copy_back", "Copy sign back / 复制告示牌背面", {}},
        ActionSpec{ActionId::SignPaste, "levischematic.sign.paste", "Paste into next sign / 粘贴到下个告示牌", {}},
        ActionSpec{
                   ActionId::SampleSecondary,
                   "levischematic.world.sample_secondary", "Sample replacement source / 采样替换来源",
                   {}
        },
        ActionSpec{ActionId::Materials, "levischematic.ui.materials", "Materials / 材料清单", {77, 76}},
        ActionSpec{
                   ActionId::SelectionManager,
                   "levischematic.ui.selection_manager", "Selection manager / 选区管理",
                   {77, 83}
        },
        ActionSpec{ActionId::PickBlock, "levischematic.survival.pick", "Pick projection / 投影取物", {-3}},
        ActionSpec{ActionId::TogglePickBlock, "levischematic.survival.pick.toggle", "Toggle pick / 取物开关", {77, -3}},
        ActionSpec{ActionId::Info, "levischematic.survival.info", "Block information / 方块信息", {73}},
        ActionSpec{
                   ActionId::InfoRelease,
                   "levischematic.survival.info.release", "Hide block information / 关闭方块信息",
                   {73},
                   Phase::Release
        },
        ActionSpec{ActionId::EasyPlace, "levischematic.survival.use", "Easy Place / 辅助放置", {-2}},
        ActionSpec{
                   ActionId::EasyPlaceRelease,
                   "levischematic.survival.use.release", "Stop Easy Place / 停止辅助放置",
                   {-2},
                   Phase::Release
        },
        ActionSpec{
                   ActionId::ToggleEasyPlace,
                   "levischematic.survival.easy.toggle", "Toggle Easy Place / 辅助放置开关",
                   {}
        },
        ActionSpec{
                   ActionId::ToggleRestriction,
                   "levischematic.survival.restrict.toggle", "Toggle placement restriction / 放置限制开关",
                   {}
        },
        ActionSpec{ActionId::NextLayerMode, "levischematic.view.mode.next", "Next layer mode / 下一分层模式", {77, 33}},
        ActionSpec{
                   ActionId::PreviousLayerMode,
                   "levischematic.view.mode.previous", "Previous layer mode / 上一分层模式",
                   {77, 34}
        },
        ActionSpec{ActionId::SetLayerHere, "levischematic.view.here", "Set layer here / 玩家所在层", {}},
        ActionSpec{ActionId::SaveSelection, "levischematic.selection.save", "Save selection / 保存选区", {17, 18, 83}},
        ActionSpec{ActionId::Verifier, "levischematic.ui.verifier", "Verifier / 校验器", {77, 86}},
        ActionSpec{ActionId::MainMenu, "levischematic.ui.open", "Main menu / 主菜单", {77}, Phase::Release},
        ActionSpec{ActionId::Settings, "levischematic.ui.settings", "Settings / 设置", {77, 67}},
        ActionSpec{ActionId::Placements, "levischematic.ui.placements", "Placements / 放置列表", {77, 80}},
        ActionSpec{
                   ActionId::PlacementSettings,
                   "levischematic.ui.placement", "Placement settings / 放置配置",
                   {109},
                   Phase::Press,
                   false, true
        },
        ActionSpec{
                   ActionId::SelectionSettings,
                   "levischematic.ui.selection", "Selection editor / 选区编辑器",
                   {106},
                   Phase::Press,
                   false, true
        },
        ActionSpec{ActionId::ToggleTool, "levischematic.tool.toggle", "Toggle tool / 工具开关", {77, 84}},
        ActionSpec{
                   ActionId::CycleMode,
                   "levischematic.tool.mode", "Cycle tool / 切换工具",
                   {17, WheelUp},
                   Phase::Press,
                   true
        },
        ActionSpec{
                   ActionId::CycleModeReverse,
                   "levischematic.tool.mode.reverse", "Cycle tool backward / 反向切换工具",
                   {17, WheelDown},
                   Phase::Press,
                   true
        },
        ActionSpec{ActionId::CycleSubmode, "levischematic.tool.submode", "Corners / Expand", {17, 77}},
        ActionSpec{
                   ActionId::Primary,
                   "levischematic.tool.primary", "Tool primary / 工具左键",
                   {-1},
                   Phase::Press,
                   true
        },
        ActionSpec{
                   ActionId::Secondary,
                   "levischematic.tool.secondary", "Tool secondary / 工具右键",
                   {-2},
                   Phase::Press,
                   true
        },
        ActionSpec{
                   ActionId::Select,
                   "levischematic.tool.select", "Select element / 选择元素",
                   {-3},
                   Phase::Press,
                   true
        },
        ActionSpec{
                   ActionId::NudgePositive,
                   "levischematic.tool.nudge.positive", "Nudge forward / 向前微调",
                   {18, WheelUp},
                   Phase::Press,
                   true
        },
        ActionSpec{
                   ActionId::NudgeNegative,
                   "levischematic.tool.nudge.negative", "Nudge backward / 向后微调",
                   {18, WheelDown},
                   Phase::Press,
                   true
        },
        ActionSpec{
                   ActionId::IncreaseLayer,
                   "levischematic.view.y.max.increment", "Layer up / 上一层",
                   {33},
                   Phase::Press,
                   false, true
        },
        ActionSpec{
                   ActionId::DecreaseLayer,
                   "levischematic.view.y.max.decrement", "Layer down / 下一层",
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
} 
