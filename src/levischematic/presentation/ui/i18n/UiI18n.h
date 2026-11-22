#pragma once
#include "levischematic/shared/i18n/I18n.h"
#include <string>
#include <string_view>
namespace levischematic::ui {
struct Row;
std::string describeRow(Row const& row);
void        reloadTranslations();
using levischematic::tr;
} // namespace levischematic::ui
