#include "InputHandler.h"

#include "levischematic/app/AppKernel.h"
#include "levischematic/app/ViewService.h"
#include "levischematic/ui/Ui.h"

#include "ll/api/input/KeyRegistry.h"
#include "ll/api/mod/NativeMod.h"

#include <Windows.h>

#include <array>
#include <utility>

namespace levischematic::input {
namespace {


struct InputRuntimeState {
    bool bindingsRegistered = false;
};


InputRuntimeState& runtimeState() {
    static InputRuntimeState state;
    return state;
}


void dispatchRegisteredAction(InputActionId action, ::FocusImpact focusImpact, ::IClientInstance& client) {
    if (ui::isOpen()) return;
    auto session = app::accessSession();
    if (session) session->input().execute(action, focusImpact, client);
}


std::array<InputHandler::InputBindingSpec, 2> const& getBindingSpecs() {
    static std::array<InputHandler::InputBindingSpec, 2> const specs{
        {
         InputHandler::InputBindingSpec{
                .id              = InputActionId::IncreaseViewMaxY,
                .registryName    = "levischematic.view.y.max.increment",
                .defaultKeyCodes = {VK_PRIOR},
                .allowRemap      = true,
                .phase           = TriggerPhase::ButtonDown,
                .executor =


                    [](InputExecutionContext const& context, ::FocusImpact, ::IClientInstance&) {
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
            }, InputHandler::InputBindingSpec{
                .id              = InputActionId::DecreaseViewMaxY,
                .registryName    = "levischematic.view.y.max.decrement",
                .defaultKeyCodes = {VK_NEXT},
                .allowRemap      = true,
                .phase           = TriggerPhase::ButtonDown,
                .executor =


                    [](InputExecutionContext const& context, ::FocusImpact, ::IClientInstance&) {
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
            }, }
    };

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
    registry.getOrCreateKey("levischematic.ui.open", {VK_F7}, true, ll::mod::NativeMod::current())
        .registerButtonDownHandler([](::FocusImpact, ::IClientInstance&) { ui::requestToggle(); });
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

} 

void registerInputDefinitions() { ensureBindingsRegistered(); }

bool InputExecutionContext::isValid() const { return viewService && static_cast<bool>(refreshProjectionState); }

void InputHandler::initialize(InputExecutionContext context) {
    ensureBindingsRegistered();
    mContext = std::move(context);
}

void InputHandler::shutdown() { mContext = {}; }

bool InputHandler::isInitialized() const { return mContext.isValid(); }

void InputHandler::execute(InputActionId action, ::FocusImpact focusImpact, ::IClientInstance& client) const {
    if (!mContext.isValid()) {
        return;
    }

    auto const* spec = findBindingSpec(action);
    if (!spec || !spec->executor) {
        return;
    }

    spec->executor(mContext, focusImpact, client);
}

char const* inputActionToString(InputActionId action) {
    switch (action) {
    case InputActionId::IncreaseViewMaxY:
        return "increase_view_max_y";
    case InputActionId::DecreaseViewMaxY:
        return "decrease_view_max_y";
    default:
        return "unknown";
    }
}

} 
