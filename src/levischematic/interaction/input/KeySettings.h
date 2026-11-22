#pragma once
#include "levischematic/interaction/input/Actions.h"

namespace levischematic::input {
enum class Activation { Press, Release, Both };
enum class KeyContext { InGame, Gui, Any };
struct KeySettings {
    Activation activateOn                           = Activation::Press;
    KeyContext context                              = KeyContext::InGame;
    bool       allowEmpty                           = false;
    bool       allowExtraKeys                       = false;
    bool       orderSensitive                       = false;
    bool       exclusive                            = false;
    bool       cancel                               = true;
    bool       operator==(KeySettings const&) const = default;
};
inline KeySettings defaultKeySettings(ActionSpec const& spec) {
    KeySettings s;
    s.activateOn     = spec.phase == Phase::Release ? Activation::Release : Activation::Press;
    s.allowExtraKeys = spec.tool || spec.id == ActionId::PickBlock || spec.id == ActionId::Info
                    || spec.id == ActionId::InfoRelease || spec.id == ActionId::EasyPlace
                    || spec.id == ActionId::EasyPlaceRelease;
    return s;
}
enum class InputContext { None, InGame, Gui };
inline bool contextAllows(KeyContext wanted, InputContext current) {
    if (current == InputContext::None) return false;
    if (wanted == KeyContext::Any) return true;
    return (wanted == KeyContext::InGame) == (current == InputContext::InGame);
}
} // namespace levischematic::input
