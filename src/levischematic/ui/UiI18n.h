#pragma once
#include <string>
#include <string_view>
namespace levischematic::ui {
struct Row;
std::string describeRow(Row const& row);


void        reloadTranslations();
std::string tr(std::string_view key);
bool uiChinese();
std::string localizedText(std::string text);
} 
