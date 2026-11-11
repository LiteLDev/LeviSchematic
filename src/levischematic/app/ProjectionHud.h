#pragma once
#include "levischematic/ui/ProjectionHudModel.h"

namespace levischematic::app {
class AppKernel;

std::optional<ui::ProjectionHudSnapshot> projectionHudSnapshot(AppKernel& kernel);
} 
