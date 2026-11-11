#include "SignHooks.h"
#include "levischematic/app/AppKernel.h"

#include "ll/api/memory/Hook.h"
#include "mc/client/gui/screens/controllers/SignScreenController.h"


namespace levischematic::hook {
namespace {
bool registered = false;
}




LL_TYPE_INSTANCE_HOOK(
    SignPasteScreenHook,
    ll::memory::HookPriority::Normal,
    SignScreenController,
    &SignScreenController::$addStaticScreenVars,
    void,
    Json::Value& globalVars
) {
    origin(globalVars);
    auto session = app::accessSession();
    auto text    = session ? session->signs().consume() : std::nullopt;
    if (text) {
        mMessage      = std::move(*text);
        mMessageDirty = true;
    }
}
void registerSignHooks() {
    if (!registered) {
        app::SignService::editorAvailable = SignPasteScreenHook::hook() == 0;
        registered                        = true;
    }
}
void unregisterSignHooks() {
    if (registered) {
        if (app::SignService::editorAvailable.exchange(false)) SignPasteScreenHook::unhook();
        registered = false;
    }
}
} 
