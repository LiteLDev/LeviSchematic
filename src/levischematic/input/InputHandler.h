#pragma once

#include <functional>
#include <string_view>
#include <vector>

#include "mc/client/game/IClientInstance.h"
#include "mc/deps/input/enums/FocusImpact.h"

namespace levischematic::app {
class ProjectionService;
class ViewService;
}

namespace levischematic::verifier {
class VerifierService;
}

namespace levischematic::input {

enum class InputActionId {
    IncreaseViewMaxY,
    DecreaseViewMaxY,
};

enum class TriggerPhase {
    ButtonDown,
    ButtonUp,
};

struct InputExecutionContext {
    app::ViewService*          viewService       = nullptr;
    app::ProjectionService*    projectionService = nullptr;
    verifier::VerifierService* verifierService   = nullptr;
    std::function<void()>      refreshProjectionState;

    [[nodiscard]] bool isValid() const;
};

class InputHandler {
public:
    using InputExecutor = std::function<void(InputExecutionContext&, ::FocusImpact, ::IClientInstance&)>;

    struct InputBindingSpec {
        InputActionId            id;
        std::string_view         registryName;
        std::vector<int>         defaultKeyCodes;
        bool                     allowRemap = true;
        TriggerPhase             phase      = TriggerPhase::ButtonDown;
        InputExecutor            executor;
    };

    void initialize(InputExecutionContext context);
    void shutdown();
    [[nodiscard]] bool isInitialized() const;
    void               execute(InputActionId action, ::FocusImpact focusImpact, ::IClientInstance& client) const;

private:
    InputExecutionContext mContext;
};

const char* inputActionToString(InputActionId action);

} // namespace levischematic::input
