#pragma once
#include "levischematic/interaction/input/ChordState.h"
#include "levischematic/interaction/input/KeySettings.h"
#include <string>
#include <vector>
class IClientInstance;
namespace levischematic::ui {
std::string                     saveBinding(IClientInstance& client, std::string const& action, std::vector<int> keys);
void                            synchronizeBindings(IClientInstance& client);
std::vector<input::BoundAction> inputBindings();
std::string                     bindingActionName(input::ActionId id);
std::string                     describeBinding(std::vector<int> const& keys);
std::vector<int>                bindingKeys(input::ActionId id);
input::KeySettings              bindingSettings(input::ActionId id);
std::string saveBindingSettings(IClientInstance& client, input::ActionId id, input::KeySettings settings);
} // namespace levischematic::ui
