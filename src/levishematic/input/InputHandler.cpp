#include "InputHandler.h"

namespace levishematic::input {

void InputHandler::executeAction(Action action) {
    switch (action) {
    case Action::TOGGLE_RENDER:
    case Action::ROTATE_CW90:
    case Action::ROTATE_CCW90:
    case Action::MIRROR_X:
    case Action::MIRROR_Z:
    case Action::MOVE_TO_CROSSHAIR:
    case Action::SET_CORNER1:
    case Action::SET_CORNER2:
        break;
    case Action::GRAB_ELEMENT:
        mGrabMode = !mGrabMode;
        break;
    case Action::OPEN_MAIN_MENU:
    case Action::OPEN_LOAD_MENU:
    case Action::OPEN_PLACEMENT_MENU:
    case Action::NONE:
    default:
        break;
    }

    if (mActionCallback) {
        mActionCallback(action);
    }
}

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
    return it != mBindings.end() ? &it->second : nullptr;
}

void InputHandler::resetDefaultBindings() {
    mBindings.clear();
    setupDefaultBindings();
}

void InputHandler::setupDefaultBindings() {
    setKeyBinding(Action::MOVE_TO_CROSSHAIR, 0x4D);
    setKeyBinding(Action::ROTATE_CW90, 0x52);
    setKeyBinding(Action::TOGGLE_RENDER, 0x56);
    setKeyBinding(Action::OPEN_MAIN_MENU, 0x4D, true);
}

void InputHandler::init() {
    if (mInitialized) {
        return;
    }
    setupDefaultBindings();
    mInitialized = true;
}

void InputHandler::shutdown() {
    mBindings.clear();
    mActionCallback = nullptr;
    mGrabMode       = false;
    mInitialized    = false;
}

const char* actionToString(Action action) {
    switch (action) {
    case Action::TOGGLE_RENDER:       return "toggle_render";
    case Action::ROTATE_CW90:         return "rotate_cw90";
    case Action::ROTATE_CCW90:        return "rotate_ccw90";
    case Action::MIRROR_X:            return "mirror_x";
    case Action::MIRROR_Z:            return "mirror_z";
    case Action::MOVE_TO_CROSSHAIR:   return "move_to_crosshair";
    case Action::SET_CORNER1:         return "set_corner1";
    case Action::SET_CORNER2:         return "set_corner2";
    case Action::GRAB_ELEMENT:        return "grab_element";
    case Action::OPEN_MAIN_MENU:      return "open_main_menu";
    case Action::OPEN_LOAD_MENU:      return "open_load_menu";
    case Action::OPEN_PLACEMENT_MENU: return "open_placement_menu";
    case Action::NONE:
    default:                          return "none";
    }
}

Action stringToAction(const std::string& name) {
    if (name == "toggle_render") return Action::TOGGLE_RENDER;
    if (name == "rotate_cw90") return Action::ROTATE_CW90;
    if (name == "rotate_ccw90") return Action::ROTATE_CCW90;
    if (name == "mirror_x") return Action::MIRROR_X;
    if (name == "mirror_z") return Action::MIRROR_Z;
    if (name == "move_to_crosshair") return Action::MOVE_TO_CROSSHAIR;
    if (name == "set_corner1") return Action::SET_CORNER1;
    if (name == "set_corner2") return Action::SET_CORNER2;
    if (name == "grab_element") return Action::GRAB_ELEMENT;
    if (name == "open_main_menu") return Action::OPEN_MAIN_MENU;
    if (name == "open_load_menu") return Action::OPEN_LOAD_MENU;
    if (name == "open_placement_menu") return Action::OPEN_PLACEMENT_MENU;
    return Action::NONE;
}

} // namespace levishematic::input
