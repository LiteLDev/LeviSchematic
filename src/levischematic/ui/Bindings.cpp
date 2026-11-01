#include "Bindings.h"
#include "levischematic/LeviSchematic.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/input/ClientInputHandler.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/input/Keymapping.h"
#include "mc/client/options/IOptionRegistry.h"
#include <Windows.h>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>

namespace levischematic::ui {
namespace {
using Overrides = std::map<std::string, std::vector<int>>;
Overrides                              overrides;
bool                                   loaded = false;
std::weak_ptr<KeyboardRemappingLayout> applied;
std::filesystem::path path() { return LeviSchematic::getInstance().getSelf().getConfigDir() / "ui-keybindings.json"; }
bool                  ownAction(std::string const& action) {
    return action.starts_with("key." + LeviSchematic::getInstance().getSelf().getName() + ".");
}
void load() {
    if (loaded) return;
    loaded = true;
    try {
        std::ifstream input(path());
        if (!input) return;
        auto data = nlohmann::json::parse(input).get<Overrides>();
        for (auto const& [action, keys] : data)
            if (!ownAction(action) || keys.size() > 1) throw std::runtime_error("Invalid key binding file");
        overrides = std::move(data);
    } catch (std::exception const& e) {
        LeviSchematic::getInstance().getSelf().getLogger().warn("UI key bindings: {}", e.what());
    }
}
} 
std::string saveBinding(IClientInstance& client, std::string const& action, std::vector<int> keys) {
    load();
    if (!ownAction(action) || keys.size() > 1) return "Invalid action or binding.";
    auto layout = client.getOptions().getCurrentKeyboardRemapping();
    if (!layout) return "Keyboard layout is unavailable.";
    auto updated    = overrides;
    updated[action] = keys;
    try {
        auto file       = path();
        auto temporary  = file;
        temporary      += ".tmp";
        std::filesystem::create_directories(file.parent_path());
        {
            std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
            out << nlohmann::json(updated).dump(2);
            out.flush();
            if (!out) return "Cannot write key bindings.";
        }
        if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return "Cannot replace key bindings file.";
        overrides = std::move(updated);
        layout->setMapping(action, keys);
        if (client.getInput()) client.getInput()->updateInputMapping();
        return {};
    } catch (std::exception const& e) {
        return e.what();
    }
}
void synchronizeBindings(IClientInstance& client) {
    load();
    auto layout = client.getOptions().getCurrentKeyboardRemapping();
    if (!layout || applied.lock() == layout) return;
    
    for (auto const& [action, keys] : overrides) {
        bool found = false;
        for (auto const& mapping : *layout->mKeymappings)
            if (*mapping.mAction == action) found = true;
        if (!found) return;
    }
    for (auto const& [action, keys] : overrides) layout->setMapping(action, keys);
    if (!overrides.empty() && client.getInput()) client.getInput()->updateInputMapping();
    applied = layout;
}
} 
