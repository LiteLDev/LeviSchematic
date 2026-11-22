#pragma once
#include "levischematic/presentation/ui/model/Ui.h"
#include <filesystem>
#include <optional>
namespace levischematic::ui {
void                                 populateBrowserPage(std::shared_ptr<Page> const& page, PageId id);
std::optional<std::filesystem::path> currentAreaSelectionFile();
void                                 saveCurrentAreaSelection();
} // namespace levischematic::ui
