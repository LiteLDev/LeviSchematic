#pragma once
#include "levischematic/ui/HudLayout.h"
class UIRenderContext;
namespace levischematic::hook {
ui::HudCanvas drawProjectionHuds(UIRenderContext&, app::Config::Hud const&, bool preview = false, int selected = -1, bool paint = true);
void registerProjectionHudHooks();
void unregisterProjectionHudHooks();
} 
