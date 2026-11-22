#pragma once
#include "levischematic/interaction/input/Actions.h"
#include "levischematic/interaction/input/KeySettings.h"
#include <algorithm>
#include <optional>
#include <set>

namespace levischematic::input {
struct BoundAction {
    ActionId                   id;
    std::vector<int>           keys;
    Phase                      phase;
    bool                       tool;
    bool                       sdkCallback;
    std::optional<KeySettings> settings;
    KeySettings                effective() const { return settings ? *settings : defaultKeySettings(actionSpec(id)); }
};
struct InputDecision {
    std::optional<ActionId> action;
    bool                    consume = false;
    bool                    gui     = false;
};
class ChordState {
    std::set<int>      held, consumed, suppressed;
    std::vector<int>   order;
    std::set<ActionId> bothFired;

    bool fullyHeld(BoundAction const& b) const {
        return !b.keys.empty() && std::all_of(b.keys.begin(), b.keys.end(), [&](int k) { return held.contains(k); });
    }
    bool inOrder(std::vector<int> const& keys) const {
        size_t next = 0;
        for (int k : order)
            if (next < keys.size() && k == keys[next]) ++next;
        return next == keys.size();
    }

public:
    bool isHeld(int key) const { return held.contains(key); }
    void reset() {
        held.clear();
        consumed.clear();
        suppressed.clear();
        order.clear();
        bothFired.clear();
    }
    void deactivate() {
        held.clear();
        order.clear();
        bothFired.clear();
        suppressed = consumed;
    }
    InputDecision feed(int key, bool down, bool active, bool tool, std::vector<BoundAction> const& bindings) {
        return feed(key, down, active ? InputContext::InGame : InputContext::None, tool, bindings);
    }
    InputDecision feed(int key, bool down, InputContext context, bool tool, std::vector<BoundAction> const& bindings) {
        bool wasHeld = held.contains(key);
        bool paired  = consumed.contains(key);
        if (context == InputContext::None) {
            deactivate();
            if (!down) consumed.erase(key);
            return {{}, paired};
        }
        auto eligible = [&](BoundAction const& b) {
            return !b.sdkCallback && (!b.tool || tool) && contextAllows(b.effective().context, context);
        };
        if (down) {
            for (auto const& binding : bindings) {
                if (binding.effective().activateOn == Activation::Press || !eligible(binding)) continue;
                if (binding.id == ActionId::InfoRelease || binding.id == ActionId::EasyPlaceRelease) continue;
                if (std::find(binding.keys.begin(), binding.keys.end(), key) != binding.keys.end()) continue;
                for (int k : binding.keys)
                    if (held.contains(k)) suppressed.insert(k);
            }
            if (held.insert(key).second) order.push_back(key);
        }
        InputDecision result{{}, paired, context == InputContext::Gui};
        bool          cancel = false;
        size_t        best   = 0;
        for (auto const& binding : bindings) {
            if (!eligible(binding)) continue;
            auto s = binding.effective();
            if (binding.keys.empty()) {
                if (!s.allowEmpty || down || held.size() != 1 || !held.contains(key)) continue;
                if (s.activateOn == Activation::Release) continue;
                if (!result.action) {
                    result.action = binding.id;
                    cancel        = false;
                }
                continue;
            }
            if (std::find(binding.keys.begin(), binding.keys.end(), key) == binding.keys.end()) continue;
            if (!fullyHeld(binding)) continue;
            if (!s.allowExtraKeys && held.size() != binding.keys.size()) continue;
            if (s.orderSensitive && !inOrder(binding.keys)) continue;
            if (s.exclusive && std::any_of(bindings.begin(), bindings.end(), [&](BoundAction const& other) {
                    return &other != &binding && eligible(other) && fullyHeld(other) && other.keys != binding.keys;
                }))
                continue;
            bool onPress = s.activateOn != Activation::Release, onRelease = s.activateOn != Activation::Press;
            if (down && !onPress) {
                if (s.cancel) {
                    consumed.insert(key);
                    result.consume = true;
                }
                continue;
            }
            if (!down && !onRelease) continue;
            bool again = !down && s.activateOn == Activation::Both && bothFired.contains(binding.id);
            if ((down && wasHeld) || (suppressed.contains(key) && !again)) continue;
            bool nudge      = binding.id == ActionId::NudgePositive || binding.id == ActionId::NudgeNegative;
            bool toolSelect = binding.id == ActionId::Select && tool;
            if (binding.keys.size() > best || (binding.keys.size() == best && (nudge || toolSelect))) {
                best          = binding.keys.size();
                result.action = binding.id;
                cancel        = s.cancel;
            }
        }
        if (result.action) {
            if (cancel) {
                result.consume = down || paired;
                consumed.insert(key);
            }
            for (auto const& binding : bindings)
                if (binding.id == *result.action) {
                    if (binding.keys.size() > 1)
                        for (int k : binding.keys) suppressed.insert(k);
                    if (binding.effective().activateOn == Activation::Both) {
                        if (down) bothFired.insert(binding.id);
                        else bothFired.erase(binding.id);
                    }
                }
        }
        if (!down) {
            held.erase(key);
            std::erase(order, key);
            consumed.erase(key);
            suppressed.erase(key);
        }
        return result;
    }
};
} // namespace levischematic::input
