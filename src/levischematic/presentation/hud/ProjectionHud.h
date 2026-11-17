#pragma once
#include "levischematic/presentation/hud/model/ProjectionHudModel.h"

namespace levischematic::app {
class AppKernel;
std::optional<ui::ProjectionHudSnapshot> projectionHudSnapshot(AppKernel& kernel);
}
