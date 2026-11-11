#pragma once
#include "Ui.h"
namespace levischematic::ui {
void populateOrganizationPage(std::shared_ptr<Page> const&, PageId);
void appendPlacementOrganization(std::shared_ptr<Page> const&, uint32_t);
void appendSelectionOrganization(std::shared_ptr<Page> const&);
} 
