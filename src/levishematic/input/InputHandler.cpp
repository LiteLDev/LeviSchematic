#include "InputHandler.h"

#include "levishematic/core/DataManager.h"
#include "levishematic/schematic/placement/PlacementManager.h"

namespace levishematic::input {

// ================================================================
// 单例
// ================================================================
InputHandler& InputHandler::getInstance() {
    static InputHandler instance;
    return instance;
}

// ================================================================
// 动作执行
//
// 当前通过命令系统触发，未来通过热键触发。
// 需要 RenderChunkCoordinator 的操作暂时只记录日志，
// 实际的渲染刷新由命令回调负责。
// ================================================================
void InputHandler::executeAction(Action action) {
    auto& dm = core::DataManager::getInstance();
    auto& pm = dm.getPlacementManager();
    auto* sel = pm.getSelected();

    switch (action) {
    case Action::TOGGLE_RENDER:
        if (sel) sel->toggleRender();
        break;
    case Action::ROTATE_CW90:
        if (sel) sel->rotateCW90();
        break;
    case Action::ROTATE_CCW90:
        if (sel) sel->rotateCCW90();
        break;
    case Action::MIRROR_X:
        if (sel) sel->toggleMirrorX();
        break;
    case Action::MIRROR_Z:
        if (sel) sel->toggleMirrorZ();
        break;
    case Action::MOVE_TO_CROSSHAIR:
        // Phase 4+: 需要获取玩家准心位置
        break;
    case Action::SET_CORNER1:
        // Phase 4: 通过 SelectionManager 设置角点1（需要外部传入坐标）
        // 实际的鼠标 Hook 在 render_wireframe_test.cpp 中实现
        break;
    case Action::SET_CORNER2:
        // Phase 4: 通过 SelectionManager 设置角点2（需要外部传入坐标）
        // 实际的鼠标 Hook 在 render_wireframe_test.cpp 中实现
        break;
    case Action::GRAB_ELEMENT:
        // Phase 4+: 拖拽模式
        mGrabMode = !mGrabMode;
        break;
    case Action::OPEN_MAIN_MENU:
    case Action::OPEN_LOAD_MENU:
    case Action::OPEN_PLACEMENT_MENU:
        // Phase 6: GUI 操作
        break;
    case Action::NONE:
    default:
        break;
    }

    // 通知回调（如果有 GUI 监听）
    if (mActionCallback) {
        mActionCallback(action);
    }
}

// ================================================================
// 热键管理（Phase 4+ 才有实际 Hook 实现）
// ================================================================
void InputHandler::setKeyBinding(Action action, int keyCode, bool ctrl, bool shift, bool alt) {
    KeyBinding binding;
    binding.name    = actionToString(action);
    binding.keyCode = keyCode;
    binding.ctrl    = ctrl;
    binding.shift   = shift;
    binding.alt     = alt;
    binding.action  = action;
    binding.enabled = true;

    mBindings[action] = std::move(binding);
}

const KeyBinding* InputHandler::getKeyBinding(Action action) const {
    auto it = mBindings.find(action);
    return (it != mBindings.end()) ? &it->second : nullptr;
}

void InputHandler::resetDefaultBindings() {
    mBindings.clear();
    setupDefaultBindings();
}

// ================================================================
// 默认热键表
//
// 键码参考 Windows VK 码（Phase 4+ 时对应 BE 输入系统）
// 当前仅作为配置预设，尚未注册实际 Hook
// ================================================================
void InputHandler::setupDefaultBindings() {
    // M 键 → 移动投影到准心
    setKeyBinding(Action::MOVE_TO_CROSSHAIR, 0x4D);

    // R 键 → 顺时针旋转
    setKeyBinding(Action::ROTATE_CW90, 0x52);

    // V 键 → 开关渲染
    setKeyBinding(Action::TOGGLE_RENDER, 0x56);

    // Ctrl+M → 打开主菜单
    setKeyBinding(Action::OPEN_MAIN_MENU, 0x4D, true);
}

// ================================================================
// 生命周期
// ================================================================
void InputHandler::init() {
    if (mInitialized) return;
    setupDefaultBindings();
    mInitialized = true;
    // Phase 4+: 在此注册键盘/鼠标 Hook
}

void InputHandler::shutdown() {
    mBindings.clear();
    mActionCallback = nullptr;
    mGrabMode = false;
    mInitialized = false;
    // Phase 4+: 在此注销 Hook
}

// ================================================================
// 动作名称工具
// ================================================================
const char* actionToString(Action action) {
    switch (action) {
    case Action::TOGGLE_RENDER:      return "toggle_render";
    case Action::ROTATE_CW90:        return "rotate_cw90";
    case Action::ROTATE_CCW90:       return "rotate_ccw90";
    case Action::MIRROR_X:           return "mirror_x";
    case Action::MIRROR_Z:           return "mirror_z";
    case Action::MOVE_TO_CROSSHAIR:  return "move_to_crosshair";
    case Action::SET_CORNER1:        return "set_corner1";
    case Action::SET_CORNER2:        return "set_corner2";
    case Action::GRAB_ELEMENT:       return "grab_element";
    case Action::OPEN_MAIN_MENU:     return "open_main_menu";
    case Action::OPEN_LOAD_MENU:     return "open_load_menu";
    case Action::OPEN_PLACEMENT_MENU:return "open_placement_menu";
    case Action::NONE:
    default:                         return "none";
    }
}

Action stringToAction(const std::string& name) {
    if (name == "toggle_render")      return Action::TOGGLE_RENDER;
    if (name == "rotate_cw90")        return Action::ROTATE_CW90;
    if (name == "rotate_ccw90")       return Action::ROTATE_CCW90;
    if (name == "mirror_x")           return Action::MIRROR_X;
    if (name == "mirror_z")           return Action::MIRROR_Z;
    if (name == "move_to_crosshair")  return Action::MOVE_TO_CROSSHAIR;
    if (name == "set_corner1")        return Action::SET_CORNER1;
    if (name == "set_corner2")        return Action::SET_CORNER2;
    if (name == "grab_element")       return Action::GRAB_ELEMENT;
    if (name == "open_main_menu")     return Action::OPEN_MAIN_MENU;
    if (name == "open_load_menu")     return Action::OPEN_LOAD_MENU;
    if (name == "open_placement_menu")return Action::OPEN_PLACEMENT_MENU;
    return Action::NONE;
}

} // namespace levishematic::input
