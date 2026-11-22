#pragma once
#include "levischematic/presentation/hud/model/ProjectionHudModel.h"
#include "levischematic/schematic/placement/model/EditorState.h"

namespace levischematic::app {
class AppKernel;
std::string                              toolNameKey(editor::ToolMode mode);
std::optional<ui::ProjectionHudSnapshot> projectionHudSnapshot(AppKernel& kernel);
} // namespace levischematic::app
