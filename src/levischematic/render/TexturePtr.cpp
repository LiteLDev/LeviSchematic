#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"

#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/resource/ResourceLocation.h"

#include <memory>

namespace mce {



TexturePtr::TexturePtr()
: mClientTexture{},
  mResourceLocationPtr(std::make_shared<ResourceLocation>(Core::PathView{""})) {}

} 
