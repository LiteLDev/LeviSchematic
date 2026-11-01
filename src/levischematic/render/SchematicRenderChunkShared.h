#pragma once

#include "mc/client/renderer/chunks/RenderChunkShared.h"
#include "mc/world/level/SubChunkPos.h"

#include <memory>

namespace levischematic::render {


using SchematicRenderChunkShared = ::RenderChunkShared;


[[nodiscard]] inline std::shared_ptr<SchematicRenderChunkShared>
makeSchematicRenderChunkShared(::SubChunkPos const& chunkPos) {
    return std::make_shared<SchematicRenderChunkShared>(chunkPos);
}

} 
