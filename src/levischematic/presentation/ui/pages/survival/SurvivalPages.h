#pragma once
#include "levischematic/presentation/ui/model/Ui.h"
#include <filesystem>
namespace levischematic::ui {
void populateSurvivalPage(std::shared_ptr<Page> const&, PageId);
void openPlacementMaterials();
void openFileMaterials(std::filesystem::path const& file);
} // namespace levischematic::ui
