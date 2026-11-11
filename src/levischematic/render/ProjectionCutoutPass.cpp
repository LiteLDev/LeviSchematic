#include "BlockEntityHighlight.h"
#include "NativeChunkGeometryDescription.h"
#include "ll/api/memory/Hook.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/minecraft_renderer/game/TerrainLayer.h"
#include <array>
#include <cstring>

namespace levischematic::render {
namespace {
std::uintptr_t alphaEntry = 0;
std::uintptr_t opaqueEntry = 0;
std::uintptr_t chunkEntry = 0;
bool installed = false;
static_assert(sizeof(NativeChunkGeometryDescription) == 240);
static_assert(offsetof(NativeChunkGeometryDescription, mTerrainLayer) == 0x48);
static_assert(offsetof(NativeChunkGeometryDescription, mIsHighlighted) == 0xe0);





LL_STATIC_HOOK(ProjectionCutoutChunkHook, ll::memory::HookPriority::Normal, chunkEntry,
    void, void* self, void const* context, NativeChunkGeometryDescription const& description) {
    auto const* layer = description.mTerrainLayer;
    std::uint32_t lighting;
    std::memcpy(&lighting, static_cast<std::byte const*>(context) + 112, sizeof(lighting));
    bool const cutout = layer && (layer->name.get().find("alpha") != std::string_view::npos
        || layer->name.get() == "terrain_doubleside");
    if (!description.mIsHighlighted || !cutout || lighting == 0) {
        origin(self, context, description);
        return;
    }
    alignas(8) std::array<std::byte, 120> raster;
    std::memcpy(raster.data(), context, raster.size());
    std::memset(raster.data() + 112, 0, 5);
    origin(self, raster.data(), description);
}

LL_STATIC_HOOK(
    ProjectionCutoutPassHook,
    ll::memory::HookPriority::Normal,
    alphaEntry,
    void,
    void* declarator,
    void const* context,
    int sortMode,
    HashedString const& pass,
    std::string const& name,
    void const* cameras,
    void const* color,
    void const* optionalColor,
    void const* depth,
    void const* buffer1,
    void const* buffer2,
    void const* texture1,
    void const* texture2,
    void const* texture3,
    void const* texture4
) {
    if (pass.getString() == "ForwardPBRAlphaTest") {
        static HashedString const rasterPass{"AlphaTest"};
        origin(declarator, context, sortMode, rasterPass, name + "/ProjectionRasterAlpha", cameras,
               color, optionalColor, depth, buffer1, buffer2, texture1, texture2, texture3, texture4);
    }
    origin(declarator, context, sortMode, pass, name, cameras, color, optionalColor, depth,
           buffer1, buffer2, texture1, texture2, texture3, texture4);
}
LL_STATIC_HOOK(
    ProjectionOpaquePassHook, ll::memory::HookPriority::Normal, opaqueEntry, void,
    void* declarator, void const* context, int sortMode, HashedString const& pass,
    std::string const& name, void const* cameras, void const* color, void const* optionalColor,
    void const* depth, void const* buffer1, void const* buffer2, void const* texture1,
    void const* texture2, void const* texture3, void const* texture4
) {
    if (pass.getString() == "ForwardPBROpaque") {
        static HashedString const rasterPass{"Opaque"};
        origin(declarator, context, sortMode, rasterPass, name + "/ProjectionRasterOpaque", cameras,
               color, optionalColor, depth, buffer1, buffer2, texture1, texture2, texture3, texture4);
    }
    origin(declarator, context, sortMode, pass, name, cameras, color, optionalColor, depth,
           buffer1, buffer2, texture1, texture2, texture3, texture4);
}
} 
bool registerProjectionCutoutPass() {
    if (installed) return true;
    
    
    alphaEntry = resolveProjectionEntry("",
        "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 98 0C 00 00 48 8D AC 24 80 00 00 00 "
        "0F 29 B5 00 0C 00 00 48 C7 85 F8 0B 00 00 FE FF FF FF 44 89 85 AC 0B 00 00 "
        "48 89 8D 28 0B 00 00 49 83 C1 08 4C 89 8D 48 0B 00 00 0F 57 F6 4C 8B 6A 10 "
        "48 8B 42 18 48 89 85 40 0B 00 00 48 8D 42 28 48 89 85 30 0B 00 00 48 89 95 C0 0B 00 00 "
        "48 8D 42 60 48 89 85 38 0B 00 00 EB 10 66 0F 1F 84 00 00 00 00 00 "
        "49 81 C5 90 02 00 00 4C 3B AD 40 0B 00 00 0F 84 9C 1D 00 00 49 8D 85 40 02 00 00 "
        "48 8B 8D C0 0B 00 00 48 8B 71 08 48 8D 0D 23 D3 80 03");
    chunkEntry = resolveProjectionEntry("", "55 41 57 41 56 41 55 41 54 56 57 53 B8 08 18 00 00");
    auto callSite = resolveProjectionEntry("",
        "E8 BC AA 0A 00 90 BA 20 00 00 00 48 89 D9 E8 ? ? ? ? 66 0F 57 C0 66");
    if (callSite) {
        std::int32_t displacement;
        std::memcpy(&displacement, reinterpret_cast<void const*>(callSite + 1), sizeof(displacement));
        opaqueEntry = callSite + 5 + displacement;
    }
    bool const alphaHooked = alphaEntry && ProjectionCutoutPassHook::hook() == 0;
    bool const opaqueHooked = opaqueEntry && ProjectionOpaquePassHook::hook() == 0;
    bool const chunkHooked = chunkEntry && ProjectionCutoutChunkHook::hook() == 0;
    installed = alphaHooked && opaqueHooked && chunkHooked;
    if (!installed) {
        if (alphaHooked) ProjectionCutoutPassHook::unhook();
        if (opaqueHooked) ProjectionOpaquePassHook::unhook();
        if (chunkHooked) ProjectionCutoutChunkHook::unhook();
    }
    return installed;
}
void unregisterProjectionCutoutPass() {
    if (installed) ProjectionCutoutPassHook::unhook();
    if (installed) ProjectionOpaquePassHook::unhook();
    if (installed) ProjectionCutoutChunkHook::unhook();
    installed = false;
    alphaEntry = 0;
    opaqueEntry = chunkEntry = 0;
}
} 
