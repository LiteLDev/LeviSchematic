#pragma once
#include <string>
#include <vector>
class IClientInstance;
namespace levischematic::ui {


std::string saveBinding(IClientInstance& client, std::string const& action, std::vector<int> keys);
void        synchronizeBindings(IClientInstance& client);
} 
