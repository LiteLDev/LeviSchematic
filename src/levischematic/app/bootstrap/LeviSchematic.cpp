#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/app/lifecycle/AppKernel.h"
#include "levischematic/presentation/ui/model/Ui.h"
#include "ll/api/Versions.h"

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


    ui::stop();
    app::stop();
    return true;
}

}

LL_REGISTER_MOD(levischematic::LeviSchematic, levischematic::LeviSchematic::getInstance());
