#include "InputHandler.h"

#include "levischematic/app/ViewService.h"
#include "levischematic/LeviSchematic.h"

#include "ll/api/input/KeyRegistry.h"
#include "ll/api/mod/NativeMod.h"

#include <Windows.h>

#include <array>
#include <utility>

namespace levischematic::input {
namespace {
    auto& getLogger() {
    return levischematic::LeviSchematic::getInstance().getSelf().getLogger();
}

struct InputRuntimeState {
    InputHandler* activeHandler     = nullptr;
    bool          bindingsRegistered = false;
};

InputRuntimeState& runtimeState() {
    static InputRuntimeState state;
    return state;
}

void dispatchRegisteredAction(InputActionId action, ::FocusImpact focusImpact, ::IClientInstance& client) {
    auto* handler = runtimeState().activeHandler;
    if (!handler) {
        return;
    }

    handler->execute(action, focusImpact, client);
}

std::array<InputHandler::InputBindingSpec, 2> const& getBindingSpecs() {
    static const std::array<InputHandler::InputBindingSpec, 2> specs{{
        InputHandler::InputBindingSpec{
            .id             = InputActionId::IncreaseViewMaxY,
            .registryName   = "levischematic.view.y.max.increment",
            .defaultKeyCodes = {VK_PRIOR},
            .allowRemap     = true,
            .phase          = TriggerPhase::ButtonDown,
            .executor       = [](InputExecutionContext& context, ::FocusImpact, ::IClientInstance&) {
                if (!context.viewService) {
                    return;
                }

                if (!context.viewService->adjustMaxYBy(1)) {
                    return;
                }

                if (context.refreshProjectionState) {
                    context.refreshProjectionState();
                }
            },
        },
        InputHandler::InputBindingSpec{
            .id             = InputActionId::DecreaseViewMaxY,
            .registryName   = "levischematic.view.y.max.decrement",
            .defaultKeyCodes = {VK_NEXT},
            .allowRemap     = true,
            .phase          = TriggerPhase::ButtonDown,
            .executor       = [](InputExecutionContext& context, ::FocusImpact, ::IClientInstance&) {
                getLogger().debug("down");
                if (!context.viewService) {
                    return;
                }

                if (!context.viewService->adjustMaxYBy(-1)) {
                    return;
                }

                if (context.refreshProjectionState) {
                    context.refreshProjectionState();
                }
            },
        },
    }};

    return specs;
}

InputHandler::InputBindingSpec const* findBindingSpec(InputActionId action) {
    auto const& specs = getBindingSpecs();
    for (auto const& spec : specs) {
        if (spec.id == action) {
            return &spec;
        }
    }

    return nullptr;
}

void ensureBindingsRegistered() {
    auto& state = runtimeState();
    if (state.bindingsRegistered) {
        return;
    }

    auto& registry = ll::input::KeyRegistry::getInstance();
    for (auto const& spec : getBindingSpecs()) {
        auto& keyHandle = registry.getOrCreateKey(
            spec.registryName,
            spec.defaultKeyCodes,
            spec.allowRemap,
            ll::mod::NativeMod::current()
        );

        if (spec.phase == TriggerPhase::ButtonDown) {
            keyHandle.registerButtonDownHandler(
                [action = spec.id](::FocusImpact focusImpact, ::IClientInstance& client) {
                    dispatchRegisteredAction(action, focusImpact, client);
                }
            );
            continue;
        }

        keyHandle.registerButtonUpHandler(
            [action = spec.id](::FocusImpact focusImpact, ::IClientInstance& client) {
                dispatchRegisteredAction(action, focusImpact, client);
            }
        );
    }

    state.bindingsRegistered = true;
}

} // namespace

bool InputExecutionContext::isValid() const {
    return viewService && projectionService && verifierService && static_cast<bool>(refreshProjectionState);
}

void InputHandler::initialize(InputExecutionContext context) {
    ensureBindingsRegistered();
    mContext                = std::move(context);
    runtimeState().activeHandler = this;
}

void InputHandler::shutdown() {
    if (runtimeState().activeHandler == this) {
        runtimeState().activeHandler = nullptr;
    }

    mContext = {};
}

bool InputHandler::isInitialized() const {
    return mContext.isValid();
}

void InputHandler::execute(InputActionId action, ::FocusImpact focusImpact, ::IClientInstance& client) const {
    if (!mContext.isValid()) {
        return;
    }

    auto const* spec = findBindingSpec(action);
    if (!spec || !spec->executor) {
        return;
    }

    spec->executor(const_cast<InputExecutionContext&>(mContext), focusImpact, client);
}

const char* inputActionToString(InputActionId action) {
    switch (action) {
    case InputActionId::IncreaseViewMaxY: return "increase_view_max_y";
    case InputActionId::DecreaseViewMaxY: return "decrease_view_max_y";
    default:                              return "unknown";
    }
}

} // namespace levischematic::input
