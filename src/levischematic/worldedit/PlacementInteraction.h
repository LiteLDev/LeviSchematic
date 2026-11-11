#pragma once
#include "mc/world/phys/HitResult.h"
#include <string>
class LocalPlayer;
class Block;
namespace levischematic::worldedit {
HitResult traceWorld(LocalPlayer&);

bool buildAtAim(LocalPlayer&, BlockPos const& target, Block const& expected, std::string& reason);
} 
