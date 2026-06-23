#pragma once

#include "mc/deps/core/math/Vec3.h"
#include "mc/world/level/BlockPos.h"

class Block;
class BlockActor;

namespace levischematic::schematic::block_actor {

struct BlockActorRenderDataForSchematic {
    Vec3         renderPosition;
    BlockPos     pos;
    const Block& block;
    BlockActor*  entity;
};

} // namespace levischematic::schematic::block_actor
