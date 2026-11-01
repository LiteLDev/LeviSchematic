#pragma once
#include <string>
#include <string_view>
namespace levischematic::ui {

void        reloadTranslations();
std::string tr(std::string_view key);
} 
