#include "Bindings.h"
#include "levischematic/LeviSchematic.h"
#include "levischematic/input/BindingRules.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/input/ClientInputHandler.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/input/Keymapping.h"
#include "mc/client/options/IOptionRegistry.h"
#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

namespace levischematic::ui {
namespace {
using Overrides = std::map<std::string, std::vector<int>>;
Overrides                              overrides;
bool                                   loaded = false;
std::recursive_mutex                   bindingMutex;
std::weak_ptr<KeyboardRemappingLayout> applied;
std::filesystem::path path() { return LeviSchematic::getInstance().getSelf().getConfigDir() / "ui-keybindings.json"; }
bool                  ownAction(std::string const& action) {
    return action.starts_with("key." + LeviSchematic::getInstance().getSelf().getName() + ".");
}
input::ActionSpec const* specFor(std::string const& action) {
    for (auto const& spec : input::actionSpecs())
        if (action == "key." + LeviSchematic::getInstance().getSelf().getName() + "." + std::string(spec.name))
            return &spec;
    return nullptr;
}
void load() {
    if (loaded) return;
    loaded = true;
    try {
        std::ifstream bindingFile(path());
        if (!bindingFile) return;
        auto data = nlohmann::json::parse(bindingFile).get<Overrides>();
        for (auto const& [action, keys] : data)
            if (!ownAction(action) || !input::validBinding(keys)) throw std::runtime_error("Invalid key binding file");
        overrides = std::move(data);
    } catch (std::exception const& e) {
        LeviSchematic::getInstance().getSelf().getLogger().warn("UI key bindings: {}", e.what());
    }
}
} 
std::string saveBinding(IClientInstance& client, std::string const& action, std::vector<int> keys) {
    std::lock_guard lock(bindingMutex);
    load();
    auto spec = specFor(action);
    if (!spec || !input::validBinding(keys)) return "Invalid action or binding.";
    for (auto const& other : input::actionSpecs()) {
        if (other.id == spec->id) continue;
        auto existing = bindingKeys(other.id);
        if (input::sameChord(keys, existing)) return "Binding conflicts with: " + std::string(other.label);
    }
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
        if (spec->sdkCallback)
            layout->setMapping(action, input::usesSdkCallback(*spec, keys) ? keys : std::vector<int>{});
        if (client.getInput()) client.getInput()->updateInputMapping();
        return {};
    } catch (std::exception const& e) {
        return e.what();
    }
}
void synchronizeBindings(IClientInstance& client) {
    std::lock_guard lock(bindingMutex);
    load();
    auto layout = client.getOptions().getCurrentKeyboardRemapping();
    if (!layout || applied.lock() == layout) return;
    
    for (auto const& [action, keys] : overrides) {
        if (!specFor(action)) continue;
        bool found = false;
        for (auto const& mapping : *layout->mKeymappings)
            if (*mapping.mAction == action) found = true;
        if (!found) return;
    }
    for (auto& [action, keys] : overrides) {
        auto spec = specFor(action);
        if (!spec) continue;
        
        for (auto& key : keys) {
            if (key >= -3 || key == input::WheelUp || key == input::WheelDown) continue;
            for (int button = 1; button <= 3; ++button)
                if (key == layout->_rawKeyToKey(button, RawInputType::MouseButton)) {
                    key = -button;
                    break;
                }
        }
        if (spec->sdkCallback)
            layout->setMapping(action, input::usesSdkCallback(*spec, keys) ? keys : std::vector<int>{});
    }
    if (!overrides.empty() && client.getInput()) client.getInput()->updateInputMapping();
    applied = layout;
}
std::string bindingActionName(input::ActionId id) {
    return "key." + LeviSchematic::getInstance().getSelf().getName() + "." + std::string(input::actionSpec(id).name);
}
std::vector<int> bindingKeys(input::ActionId id) {
    std::lock_guard lock(bindingMutex);
    load();
    auto it     = overrides.find(bindingActionName(id));
    auto result = it == overrides.end() ? input::actionSpec(id).keys : it->second;
    for (int& key : result) key = input::canonicalKey(key);
    return result;
}
std::vector<input::BoundAction> inputBindings() {
    std::lock_guard lock(bindingMutex);
    load();
    std::vector<input::BoundAction> result;
    for (auto const& spec : input::actionSpecs()) {
        auto keys = bindingKeys(
            spec.id == input::ActionId::InfoRelease        ? input::ActionId::Info
            : spec.id == input::ActionId::EasyPlaceRelease ? input::ActionId::EasyPlace
                                                           : spec.id
        );
        bool native = input::usesSdkCallback(spec, keys);
        result.push_back({spec.id, std::move(keys), spec.phase, spec.tool, native});
    }
    return result;
}
std::string describeBinding(std::vector<int> const& keys) {
    std::string text;
    for (int key : keys) {
        if (!text.empty()) text += "+";
        if (key == 17) text += "Ctrl";
        else if (key == 18) text += "Alt";
        else if (key == 16) text += "Shift";
        else if (key == input::WheelUp) text += "WheelUp";
        else if (key == input::WheelDown) text += "WheelDown";
        else if (key < 0) text += "Mouse" + std::to_string(-key);
        else if (key >= 65 && key <= 90) text += static_cast<char>(key);
        else text += "Key" + std::to_string(key);
    }
    return text.empty() ? "Unbound / 未绑定" : text;
}
} 
