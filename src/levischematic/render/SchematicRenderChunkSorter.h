#pragma once

#include "mc/client/renderer/chunks/RenderChunkSorter.h"
#include "mc/client/renderer/chunks/RenderChunkSorterSharedInfo.h"

#include <memory>

namespace levischematic::render {

using SchematicRenderChunkSorterSharedInfo = ::RenderChunkSorterSharedInfo;
using SchematicRenderChunkSorter           = ::RenderChunkSorter;
using SchematicRenderChunkSorterPtr        = std::unique_ptr<::RenderChunkSorter>;

} 
