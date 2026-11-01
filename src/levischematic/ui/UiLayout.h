#pragma once
#include <nlohmann/json.hpp>

namespace levischematic::ui {


nlohmann::ordered_json buildNativeDefinition();
nlohmann::ordered_json buildNativeDefinitions();
} 
