#pragma once
#include "levischematic/workspace/config/Config.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
namespace levischematic::render {
bool submitEditorHighlight(mce::ServerResourcePointer<dragon::ResolvedTextureResource> const&, app::Config const&);
void logOverlayTextureUnavailable(char const*, std::string const&);

}
