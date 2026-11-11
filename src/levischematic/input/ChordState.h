#pragma once
#include "Actions.h"
#include <algorithm>
#include <optional>
#include <set>

namespace levischematic::input {
struct BoundAction {
    ActionId         id;
    std::vector<int> keys;
    Phase            phase;
    bool             tool;
    bool             sdkCallback;
};
struct InputDecision {
    std::optional<ActionId> action;
    bool                    consume = false;
};

class ChordState {
    std::set<int> held, consumed, suppressed;

public:
    bool isHeld(int key) const { return held.contains(key); }
    void reset() {
        held.clear();
        consumed.clear();
        suppressed.clear();
    }
    
    void deactivate() {
        held.clear();
        suppressed = consumed;
    }
    InputDecision feed(int key, bool down, bool active, bool tool, std::vector<BoundAction> const& bindings) {
        bool wasHeld = held.contains(key);
        bool paired  = consumed.contains(key);
        if (!active) {
            deactivate();
            if (!down) consumed.erase(key);
            return {{}, paired};
        }
        if (down) {
            for (auto const& binding : bindings) {
                if (binding.phase != Phase::Release) continue;
                
                if (binding.id == ActionId::InfoRelease || binding.id == ActionId::EasyPlaceRelease) continue;
                if (std::find(binding.keys.begin(), binding.keys.end(), key) != binding.keys.end()) continue;
                for (int k : binding.keys)
                    if (held.contains(k)) suppressed.insert(k);
            }
            held.insert(key);
        }
        InputDecision result{{}, paired};
        size_t        best = 0;
        for (auto const& binding : bindings) {
            if (binding.sdkCallback || binding.keys.empty() || (binding.tool && !tool)) continue;
            if (std::find(binding.keys.begin(), binding.keys.end(), key) == binding.keys.end()) continue;
            bool matches =
                std::all_of(binding.keys.begin(), binding.keys.end(), [&](int k) { return held.contains(k); });
            if (!matches) continue;
            
            bool allowExtra = binding.id == ActionId::PickBlock || binding.id == ActionId::Info
                           || binding.id == ActionId::InfoRelease || binding.id == ActionId::EasyPlace
                           || binding.id == ActionId::EasyPlaceRelease;
            if (!binding.tool && !allowExtra && held.size() != binding.keys.size()) continue;
            if (binding.phase == Phase::Release && down) {
                
                consumed.insert(key);
                result.consume = true;
                continue;
            }
            if ((binding.phase == Phase::Press) != down || (down && wasHeld) || suppressed.contains(key)) continue;
            bool nudge      = binding.id == ActionId::NudgePositive || binding.id == ActionId::NudgeNegative;
            bool toolSelect = binding.id == ActionId::Select && tool;
            if (binding.keys.size() > best || (binding.keys.size() == best && (nudge || toolSelect))) {
                best          = binding.keys.size();
                result.action = binding.id;
            }
        }
        if (result.action) {
            
            
            result.consume = down || paired;
            consumed.insert(key);
            for (auto const& binding : bindings)
                if (binding.id == *result.action && binding.keys.size() > 1)
                    for (int k : binding.keys) suppressed.insert(k);
        }
        if (!down) {
            held.erase(key);
            consumed.erase(key);
            suppressed.erase(key);
        }
        return result;
    }
};
} 
