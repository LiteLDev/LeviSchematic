#include "levischematic/LeviSchematic.h"
#include "levischematic/app/AppKernel.h"
#include "levischematic/ui/Ui.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/service/GamingStatus.h"
#include "ll/api/Versions.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/server/ServerInstance.h"

#include "ll/api/mod/RegisterHelper.h"

namespace levischematic {

namespace {






void logBuildStamp() {
    LeviSchematic::getInstance().getSelf().getLogger().info(
        "LeviSchematic build stamp: {} {} (Phase 5: preview runtime lifecycle)",
        __DATE__,
        __TIME__
    );
}


} 

LeviSchematic& LeviSchematic::getInstance() {
    static LeviSchematic instance;
    return instance;
}

bool LeviSchematic::load() {
    getSelf().getLogger().debug("Loading...");
    auto const loader = ll::getLoaderVersion();
    if (loader.major != 26 || loader.minor != 60 || loader.patch < 4) {
        getSelf().getLogger().error(
            "LeviSchematic requires LeviLamina Client 26.60.4 or later in the 26.60 series. "
            "Update the loader with its matching PreLoader and bedrock-runtime-data to fix exit crashes."
        );
        return false;
    }
    logBuildStamp();
    app::load();
    return true;
}

bool LeviSchematic::enable() {
    getSelf().getLogger().debug("Enabling...");
    logBuildStamp();
    app::load();
    app::start();
    ui::start();
    return true;
}

bool LeviSchematic::disable() {
    getSelf().getLogger().debug("Disabling...");
    
    
    
    auto client = ll::service::getClientInstance();
    auto server = ll::service::getServerInstance();
    bool stopping = ll::getGamingStatus() == ll::GamingStatus::Stopping;
    bool destroying = client && client->isDestroyingGame();
    getSelf().getLogger().info("Shutdown drain: stopping={}, destroying={}, client={}, server={}",
        stopping, destroying, static_cast<bool>(client), static_cast<bool>(server));
    if (server && (stopping || destroying || !client)) server->leaveGameSync();
    ui::stop();
    app::stop();
    return true;
}

} 

LL_REGISTER_MOD(levischematic::LeviSchematic, levischematic::LeviSchematic::getInstance());
