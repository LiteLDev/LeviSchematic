#include "levischematic/LeviSchematic.h"
#include "levischematic/app/AppKernel.h"
#include "levischematic/ui/Ui.h"

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
    ui::stop();
    app::stop();
    return true;
}

} 

LL_REGISTER_MOD(levischematic::LeviSchematic, levischematic::LeviSchematic::getInstance());
