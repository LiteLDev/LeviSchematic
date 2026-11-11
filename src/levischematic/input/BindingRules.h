#pragma once
#include "Actions.h"
#include <algorithm>
#include <set>

namespace levischematic::input {
inline int canonicalKey(int key) {
    if (key == 160 || key == 161) return 16;
    if (key == 162 || key == 163) return 17;
    if (key == 164 || key == 165) return 18;
    return key;
}
inline bool validBinding(std::vector<int> const& keys) {
    if (keys.size() > 8) return false;
    std::set<int> unique;
    for (int key : keys) {
        bool valid = (key > 0 && key <= 255) || (key < 0 && key >= -255) || key == WheelUp || key == WheelDown;
        if (!valid || !unique.insert(canonicalKey(key)).second) return false;
    }
    return true;
}
inline bool usesSdkCallback(ActionSpec const& spec, std::vector<int> const& keys) {
    return spec.sdkCallback && keys.size() == 1 && keys[0] > 0;
}
inline bool sameChord(std::vector<int> a, std::vector<int> b) {
    if (a.empty() || b.empty()) return false;
    for (int& key : a) key = canonicalKey(key);
    for (int& key : b) key = canonicalKey(key);
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    return a == b;
}
} 
