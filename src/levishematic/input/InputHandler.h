#pragma once

#include <functional>

namespace levishematic::input {

enum class Action {
    ToggleRender,
    RotateClockwise90,
    RotateCounterClockwise90,
    MirrorX,
    MirrorZ,
    ToggleSelectionMode,
    None,
};

class InputHandler {
public:
    using ActionCallback = std::function<void(Action)>;

    void bind(ActionCallback callback);
    void dispatch(Action action) const;
    void shutdown();

private:
    ActionCallback mActionCallback;
};

const char* actionToString(Action action);

} // namespace levishematic::input
