#pragma once
#include "levischematic/input/ChordState.h"
#include <string>
#include <vector>
class IClientInstance;
namespace levischematic::ui {


std::string saveBinding(IClientInstance& client, std::string const& action, std::vector<int> keys);
void        synchronizeBindings(IClientInstance& client);
std::vector<input::BoundAction> inputBindings();
std::string                     bindingActionName(input::ActionId id);
std::string                     describeBinding(std::vector<int> const& keys);
std::vector<int>                bindingKeys(input::ActionId id);
} 
