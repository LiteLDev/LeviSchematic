#pragma once

#include "levischematic/render/chunk/model/SchematicRenderChunkShared.h"
#include "mc/client/renderer/chunks/RenderChunkInstanced.h"
#include "mc/deps/minecraft_renderer/game/RangeIndices.h"

#include <memory>

namespace levischematic::render {

using SchematicRenderChunkInstanced    = ::RenderChunkInstanced;
using SchematicRenderChunkInstancedPtr = std::unique_ptr<::RenderChunkInstanced>;

}
