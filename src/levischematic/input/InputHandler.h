#pragma once

#include <cstdint>
#include <functional>

#include "mc/client/game/IClientInstance.h"

namespace levischematic::app {
class ViewService;
class ToolService;
}

namespace levischematic::input {





struct InputExecutionContext {
    app::ViewService*          viewService       = nullptr;
    std::function<void()>      refreshProjectionState;
    app::ToolService*          toolService = nullptr;

    


    [[nodiscard]] bool isValid() const;
};





class InputHandler {
public:
    



    void initialize(InputExecutionContext context);
    


    void shutdown();
    


    [[nodiscard]] bool isInitialized() const;
    void               pump(::IClientInstance& client, uint64_t generation);

private:
    InputExecutionContext mContext;
};






void registerInputDefinitions();
void startInputEvents();
void stopInputEvents();
void resetInputState();

} 
