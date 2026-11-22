#include "levischematic/presentation/ui/bindings/Bindings.h"
#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/interaction/input/BindingRules.h"
#include "levischematic/shared/i18n/I18n.h"
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
Overrides                                 overrides;
std::map<std::string, input::KeySettings> settingOverrides;
bool                                      loaded = false;
std::recursive_mutex                      bindingMutex;
std::weak_ptr<KeyboardRemappingLayout>    applied;
std::filesystem::path path() { return LeviSchematic::getInstance().getSelf().getConfigDir() / "ui-keybindings.json"; }
std::filesystem::path settingsPath() {
    return LeviSchematic::getInstance().getSelf().getConfigDir() / "ui-keysettings.json";
}
constexpr char const* ActivationNames[] = {"press", "release", "both"};
constexpr char const* ContextNames[]    = {"ingame", "gui", "any"};
template <size_t N>
int indexOf(char const* const (&names)[N], std::string const& value) {
    for (size_t i = 0; i < N; ++i)
        if (value == names[i]) return static_cast<int>(i);
    throw std::runtime_error("Invalid key setting: " + value);
}
nlohmann::json encodeSettings(input::KeySettings const& s) {
    return {
        {"activateOn",     ActivationNames[static_cast<int>(s.activateOn)]},
        {"context",        ContextNames[static_cast<int>(s.context)]      },
        {"allowEmpty",     s.allowEmpty                                   },
        {"allowExtraKeys", s.allowExtraKeys                               },
        {"orderSensitive", s.orderSensitive                               },
        {"exclusive",      s.exclusive                                    },
        {"cancel",         s.cancel                                       }
    };
}
input::KeySettings decodeSettings(nlohmann::json const& v, input::KeySettings s) {
    if (v.contains("activateOn"))
        s.activateOn = static_cast<input::Activation>(indexOf(ActivationNames, v.at("activateOn")));
    if (v.contains("context")) s.context = static_cast<input::KeyContext>(indexOf(ContextNames, v.at("context")));
    s.allowEmpty     = v.value("allowEmpty", s.allowEmpty);
    s.allowExtraKeys = v.value("allowExtraKeys", s.allowExtraKeys);
    s.orderSensitive = v.value("orderSensitive", s.orderSensitive);
    s.exclusive      = v.value("exclusive", s.exclusive);
    s.cancel         = v.value("cancel", s.cancel);
    return s;
}
bool ownAction(std::string const& action) {
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
    try {
        std::ifstream settingsFile(settingsPath());
        if (!settingsFile) return;
        auto                                      data = nlohmann::json::parse(settingsFile);
        std::map<std::string, input::KeySettings> parsed;
        for (auto const& [action, value] : data.items()) {
            auto spec = specFor(action);
            if (!spec) throw std::runtime_error("Unknown action " + action);
            parsed[action] = decodeSettings(value, input::defaultKeySettings(*spec));
        }
        settingOverrides = std::move(parsed);
    } catch (std::exception const& e) {
        LeviSchematic::getInstance().getSelf().getLogger().warn("UI key settings: {}", e.what());
    }
}
bool customised(input::ActionSpec const& spec) {
    auto it =
        settingOverrides.find("key." + LeviSchematic::getInstance().getSelf().getName() + "." + std::string(spec.name));
    return it != settingOverrides.end() && it->second != input::defaultKeySettings(spec);
}
bool native(input::ActionSpec const& spec, std::vector<int> const& keys) {
    return input::usesSdkCallback(spec, keys) && !customised(spec);
}
} // namespace
std::string saveBinding(IClientInstance& client, std::string const& action, std::vector<int> keys) {
    std::lock_guard lock(bindingMutex);
    load();
    auto spec = specFor(action);
    if (!spec || !input::validBinding(keys)) return tr("ui.bindings.invalid");
    for (auto const& other : input::actionSpecs()) {
        if (other.id == spec->id) continue;
        auto existing = bindingKeys(other.id);
        if (input::sameChord(keys, existing)) return tr("ui.bindings.conflict", tr(other.label));
    }
    auto layout = client.getOptions().getCurrentKeyboardRemapping();
    if (!layout) return tr("ui.bindings.no_layout");
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
        if (spec->sdkCallback) layout->setMapping(action, native(*spec, keys) ? keys : std::vector<int>{});
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
        if (spec->sdkCallback) layout->setMapping(action, native(*spec, keys) ? keys : std::vector<int>{});
    }
    for (auto const& [action, settings] : settingOverrides) {
        auto spec = specFor(action);
        if (spec && spec->sdkCallback && !overrides.contains(action))
            layout->setMapping(action, native(*spec, spec->keys) ? spec->keys : std::vector<int>{});
    }
    if ((!overrides.empty() || !settingOverrides.empty()) && client.getInput()) client.getInput()->updateInputMapping();
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
        bool released = spec.id == input::ActionId::InfoRelease || spec.id == input::ActionId::EasyPlaceRelease;
        bool viaSdk   = native(spec, keys);
        result.push_back(
            {spec.id,
             std::move(keys),
             spec.phase,
             spec.tool,
             viaSdk,
             released ? std::nullopt : std::optional(bindingSettings(spec.id))}
        );
    }
    return result;
}
input::KeySettings bindingSettings(input::ActionId id) {
    std::lock_guard lock(bindingMutex);
    load();
    auto it = settingOverrides.find(bindingActionName(id));
    return it == settingOverrides.end() ? input::defaultKeySettings(input::actionSpec(id)) : it->second;
}
std::string saveBindingSettings(IClientInstance& client, input::ActionId id, input::KeySettings settings) {
    std::lock_guard lock(bindingMutex);
    load();
    auto action  = bindingActionName(id);
    auto updated = settingOverrides;
    if (settings == input::defaultKeySettings(input::actionSpec(id))) updated.erase(action);
    else updated[action] = settings;
    try {
        nlohmann::json data = nlohmann::json::object();
        for (auto const& [name, value] : updated) data[name] = encodeSettings(value);
        auto file       = settingsPath();
        auto temporary  = file;
        temporary      += ".tmp";
        std::filesystem::create_directories(file.parent_path());
        {
            std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
            out << data.dump(2);
            out.flush();
            if (!out) return "Cannot write key settings.";
        }
        if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return "Cannot replace key settings file.";
        settingOverrides = std::move(updated);
        auto const& spec = input::actionSpec(id);
        if (spec.sdkCallback)
            if (auto layout = client.getOptions().getCurrentKeyboardRemapping()) {
                auto keys = bindingKeys(id);
                layout->setMapping(action, native(spec, keys) ? keys : std::vector<int>{});
                if (client.getInput()) client.getInput()->updateInputMapping();
            }
        return {};
    } catch (std::exception const& e) {
        return e.what();
    }
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
    return text.empty() ? tr("ui.bindings.unbound") : text;
}
} // namespace levischematic::ui
