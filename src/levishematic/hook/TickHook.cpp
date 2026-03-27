#include "TickHook.h"

#include "levishematic/app/AppKernel.h"

#include "ll/api/memory/Hook.h"

#include "mc/scripting/ServerScriptManager.h"
#include "mc/server/ServerInstance.h"

namespace levishematic::hook {

LL_AUTO_TYPE_INSTANCE_HOOK(
    ServerStartCommandRegistration,
    HookPriority::Highest,
    ServerScriptManager,
    &ServerScriptManager::$onServerThreadStarted,
    EventResult,
    ::ServerInstance& ins
) {
    auto res = origin(ins);

    if (app::hasAppKernel()) {
        app::getAppKernel().onServerThreadStarted();
    }

    return res;
}

} // namespace levishematic::hook
