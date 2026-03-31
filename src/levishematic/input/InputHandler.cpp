#include "InputHandler.h"

namespace levishematic::input {

void InputHandler::bind(ActionCallback callback) {
    mActionCallback = std::move(callback);
}

void InputHandler::dispatch(Action action) const {
    if (mActionCallback) {
        mActionCallback(action);
    }
}

void InputHandler::shutdown() {
    mActionCallback = nullptr;
}

const char* actionToString(Action action) {
    switch (action) {
    case Action::ToggleRender:             return "toggle_render";
    case Action::RotateClockwise90:        return "rotate_cw90";
    case Action::RotateCounterClockwise90: return "rotate_ccw90";
    case Action::MirrorX:                  return "mirror_x";
    case Action::MirrorZ:                  return "mirror_z";
    case Action::ToggleSelectionMode:      return "toggle_selection_mode";
    case Action::None:
    default:                               return "none";
    }
}

} // namespace levishematic::input
