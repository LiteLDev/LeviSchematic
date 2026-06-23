#include "levischematic/schematic/block_actor/BaseRenderSchematic.h"

#include "mc/deps/core/file/PathView.h"

namespace levischematic::schematic::block_actor {

BaseRenderSchematic::ResourceLocationMap::ResourceLocationMap() = default;

BaseRenderSchematic::ResourceLocationMap::ResourceLocationMap(ResourceLocation vanillaRes)
: vanilla(vanillaRes),
  blendRes(ResourceLocation(Core::PathView(vanillaRes.mPath->value + "_blendqiu"))) {}

BaseRenderSchematic::ResourceLocationMap::ResourceLocationMap(
    ResourceLocation vanillaRes,
    ResourceLocation blendResource
)
: vanilla(std::move(vanillaRes)),
  blendRes(std::move(blendResource)) {}

BaseRenderSchematic::~BaseRenderSchematic() = default;

} // namespace levischematic::schematic::block_actor
