#include "ConfigStore.h"
#include "ConfigCompatibility.h"
#include "ll/api/Config.h"
#include <Windows.h>
#include <fstream>

namespace levischematic::app {
ConfigStore& configStore() {
    static ConfigStore store;
    return store;
}
std::string ConfigStore::initialize(std::filesystem::path path) {
    {
        std::scoped_lock lock(mMutex);
        mPath = std::move(path);
    }
    if (!std::filesystem::exists(mPath)) return save();
    auto error = reload();
    if (error.empty()) (void)applyPending();
    return error;
}
std::string ConfigStore::reload() {
    try {
        std::filesystem::path path;
        {
            std::scoped_lock lock(mMutex);
            path = mPath;
        }
        std::ifstream input(path);
        if (!input) return "Cannot open configuration file.";
        auto data = nlohmann::ordered_json::parse(input);
        applyVisibilityDefaults(data);
        applySelectionDefaults(data);
        Config value;
        auto   result = ll::reflection::deserialize(value, data);
        if (!result) return "Invalid configuration JSON fields.";
        return submit(std::move(value));
    } catch (std::exception const& e) {
        return e.what();
    }
}
std::string ConfigStore::save() const {
    try {
        std::scoped_lock lock(mMutex);
        if (mPath.empty()) return "Configuration directory is unavailable.";
        std::filesystem::create_directories(mPath.parent_path());
        auto temporary  = mPath;
        temporary      += ".tmp";
        if (!ll::config::saveConfig(mPending ? *mPending : mCurrent, temporary)) return "Cannot write configuration.";
        
        if (!MoveFileExW(temporary.c_str(), mPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return "Cannot replace configuration file.";
        return {};
    } catch (std::exception const& e) {
        return e.what();
    }
}
} 
