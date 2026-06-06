#include "levischematic/LeviSchematic.h"

#include "ll/api/mod/RegisterHelper.h"

namespace levischematic {

LeviSchematic& LeviSchematic::getInstance() {
    static LeviSchematic instance;
    return instance;
}

bool LeviSchematic::load() {
    getSelf().getLogger().debug("Loading...");
    return true;
}

bool LeviSchematic::enable() {
    getSelf().getLogger().debug("Enabling...");
    return true;
}

bool LeviSchematic::disable() {
    getSelf().getLogger().debug("Disabling...");
    return true;
}

} // namespace levischematic

LL_REGISTER_MOD(levischematic::LeviSchematic, levischematic::LeviSchematic::getInstance());
