#pragma once
// ============================================================
// InputHandler.h
// 输入处理 — 为未来 GUI 和热键系统预留接口
//
// Phase 3: 定义接口骨架
// Phase 4: 鼠标选区 Hook 已在 render_wireframe_test.cpp 中实现
//           （手持木棍左键=角点1，右键=角点2）
// Phase 5+: 实现键盘 Hook，热键绑定
//
// 预留功能：
//   - 热键绑定管理（按键→动作映射）
//   - 鼠标点击事件处理（选区角点设置）
//   - 拖拽模式（GrabMode，拖拽移动投影）
//   - GUI 触发（打开菜单/设置界面）
// ============================================================

#include "mc/world/level/BlockPos.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace levishematic::input {

// ================================================================
// 动作类型（热键可触发的操作）
// ================================================================
enum class Action {
    // 投影操作
    TOGGLE_RENDER,          // 开关投影渲染
    ROTATE_CW90,            // 顺时针旋转 90°
    ROTATE_CCW90,           // 逆时针旋转 90°
    MIRROR_X,               // X 轴镜像
    MIRROR_Z,               // Z 轴镜像
    MOVE_TO_CROSSHAIR,      // 移动投影到准心位置

    // 选区操作（Phase 4）
    SET_CORNER1,            // 设置选区角点 1
    SET_CORNER2,            // 设置选区角点 2
    GRAB_ELEMENT,           // 拖拽模式

    // GUI 操作（Phase 6）
    OPEN_MAIN_MENU,         // 打开主菜单
    OPEN_LOAD_MENU,         // 打开加载菜单
    OPEN_PLACEMENT_MENU,    // 打开放置设置菜单

    // 杂项
    NONE
};

// ================================================================
// 热键绑定（为未来热键系统预留）
// ================================================================
struct KeyBinding {
    std::string name;       // 显示名称
    int         keyCode;    // 按键码（参照 BE 键码表）
    bool        ctrl;       // 是否需要 Ctrl
    bool        shift;      // 是否需要 Shift
    bool        alt;        // 是否需要 Alt
    Action      action;     // 绑定的动作
    bool        enabled;    // 是否启用
};

// ================================================================
// InputHandler — 输入处理器
//
// 当前（Phase 3）：
//   仅提供接口定义和动作执行框架。
//   实际的 Hook 注册在 Phase 4+ 实现。
//
// 未来（Phase 4+）：
//   Hook 鼠标/键盘输入事件，
//   根据 KeyBinding 表触发对应 Action。
// ================================================================
class InputHandler {
public:
    static InputHandler& getInstance();

    // ---- 动作执行 ----
    // 执行指定动作（命令系统和未来的热键均可调用）
    void executeAction(Action action);

    // ---- 热键管理（Phase 4+ 实现）----

    // 设置热键绑定
    void setKeyBinding(Action action, int keyCode, bool ctrl = false, bool shift = false, bool alt = false);

    // 获取动作的热键绑定
    const KeyBinding* getKeyBinding(Action action) const;

    // 重置所有热键为默认值
    void resetDefaultBindings();

    // ---- 回调注册（为 GUI 预留）----
    using ActionCallback = std::function<void(Action)>;

    // 注册动作回调（GUI 可以注册回调来响应输入事件）
    void setActionCallback(ActionCallback cb) { mActionCallback = std::move(cb); }

    // ---- 状态查询 ----
    bool isGrabMode() const { return mGrabMode; }
    void setGrabMode(bool enabled) { mGrabMode = enabled; }

    // ---- 生命周期 ----
    void init();
    void shutdown();

private:
    InputHandler() = default;

    // 默认热键表
    void setupDefaultBindings();

    std::unordered_map<Action, KeyBinding> mBindings;
    ActionCallback                          mActionCallback;
    bool                                    mGrabMode = false;
    bool                                    mInitialized = false;
};

// ================================================================
// 动作名称工具（用于日志/配置）
// ================================================================
const char* actionToString(Action action);
Action      stringToAction(const std::string& name);

} // namespace levishematic::input
