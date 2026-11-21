#include "levischematic/render/blockentity/BlockEntityHighlight.h"
#include "levischematic/render/blockentity/BlockEntityHighlightPolicy.h"
#include "levischematic/render/chunk/submission/NativeChunkGeometryDescription.h"

#include "ll/api/memory/Hook.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/core_graphics/enums/TextureFormat.h"
#include "mc/deps/minecraft_renderer/game/TerrainLayer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <type_traits>
#include <vector>

namespace levischematic::render {
namespace {
std::uintptr_t           highlightEntry            = 0;
std::uintptr_t           imageEntry                = 0;
std::uintptr_t           chunkEntry                = 0;
std::uintptr_t           alphaEntry                = 0;
std::uintptr_t           opaqueEntry               = 0;
bool                     installed                 = false;
thread_local bool        promoteHighlightTarget    = false;
thread_local void const* coverageTarget            = nullptr;
thread_local bool        markersPending            = false;
thread_local bool        inHighlightEffect         = false;
std::uintptr_t           selectionTransparentEntry = 0;


struct NativeImageDescription {
    std::uint16_t             width, height;
    mce::TextureFormat        format;
    std::array<std::byte, 12> sampler;
    std::uint32_t             mips, usage;
};
static_assert(sizeof(NativeImageDescription) == 28);
static_assert(offsetof(NativeImageDescription, format) == 4);
static_assert(offsetof(NativeImageDescription, mips) == 20);


struct NativeColorBinding {
    std::uint64_t            outputName;
    void const*              resource;
    std::array<std::byte, 8> sliceData;
};
static_assert(sizeof(NativeColorBinding) == 24);
static_assert(offsetof(NativeColorBinding, resource) == 8);


enum class NativeDepthComparison : std::uint8_t { Equal = 3, LessEqual = 4 };
struct NativeDepthState {
    bool                  enabled;
    bool                  writeDepth;
    NativeDepthComparison comparison;
};
static_assert(sizeof(NativeDepthState) == 3);
struct alignas(4) NativePassStateOverride {
    std::array<std::byte, 8>        blend{};
    std::optional<std::uint8_t>     colorWriteMask;
    std::array<std::byte, 2>        padding{};
    std::array<std::byte, 8>        cull{};
    std::optional<NativeDepthState> depth;
    std::array<std::byte, 96>       remaining{};
};
static_assert(sizeof(NativePassStateOverride) == 120);
static_assert(offsetof(NativePassStateOverride, colorWriteMask) == 8);
static_assert(offsetof(NativePassStateOverride, depth) == 20);
static_assert(sizeof(std::optional<NativePassStateOverride>) == 124);

struct NativePassState {
    bool                  colorWrite;
    bool                  depthWrite;
    NativeDepthComparison comparison;
};

struct alignas(8) NativePassContext {
    std::array<std::byte, 40>              borrowed{};
    std::optional<NativePassStateOverride> state;
    std::array<std::byte, 4>               padding{};
    explicit NativePassContext(void const* source) { std::memcpy(this, source, sizeof(*this)); }
    NativePassContext(void const* source, NativePassState pass) {
        std::memcpy(this, source, sizeof(*this));
        if (!state) state.emplace();
        if (!pass.colorWrite) state->colorWriteMask = 0;
        state->depth = NativeDepthState{true, pass.depthWrite, pass.comparison};
    }
    NativePassContext(void const* source, bool prepass)
    : NativePassContext(
          source,
          prepass ? NativePassState{false, true, NativeDepthComparison::LessEqual}
                  : NativePassState{true, false, NativeDepthComparison::Equal}
      ) {}
};
static_assert(std::is_trivially_copyable_v<NativePassContext>);
static_assert(sizeof(NativePassContext) == 168);
static_assert(offsetof(NativePassContext, state) == 40);
static_assert(sizeof(NativeChunkGeometryDescription) == 240);
static_assert(offsetof(NativeChunkGeometryDescription, mIsHighlighted) == 224);

struct alignas(8) BorrowedPassSet {
    std::array<std::byte, 656> bytes;
};
static_assert(sizeof(BorrowedPassSet) == 656);

class ProjectionPassContext {
    std::vector<BorrowedPassSet>            views;
    std::vector<std::vector<std::uint16_t>> labels;

public:
    NativePassContext context;

    ProjectionPassContext(void const* source, bool coverage)
    : ProjectionPassContext(NativePassContext(source, coverage), [coverage](auto original) {
          return detail::projectionPassViews(original, coverage);
      }) {}

    ProjectionPassContext(ProjectionPassContext const&)            = delete;
    ProjectionPassContext& operator=(ProjectionPassContext const&) = delete;

    template <class Labels>
    ProjectionPassContext(NativePassContext pass, Labels&& select) : context(pass) {
        std::array<std::uintptr_t, 3> header;
        std::memcpy(header.data(), context.borrowed.data() + 16, sizeof(header));
        if (!header[0] || header[0] > header[1] || header[1] > header[2]
            || (header[1] - header[0]) % sizeof(BorrowedPassSet) != 0)
            return;
        auto const count = (header[1] - header[0]) / sizeof(BorrowedPassSet);
        views.resize(count);
        labels.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            std::memcpy(
                &views[i],
                reinterpret_cast<void const*>(header[0] + i * sizeof(BorrowedPassSet)),
                sizeof(BorrowedPassSet)
            );
            std::array<std::uintptr_t, 3> ids;
            std::memcpy(ids.data(), views[i].bytes.data() + 576, sizeof(ids));
            std::span<std::uint16_t const> original;
            if (ids[0] && ids[0] <= ids[1] && ids[1] <= ids[2] && (ids[1] - ids[0]) % sizeof(std::uint16_t) == 0)
                original = {reinterpret_cast<std::uint16_t const*>(ids[0]), (ids[1] - ids[0]) / sizeof(std::uint16_t)};
            labels.push_back(select(original));
            auto const& values = labels.back();
            ids                = {
                reinterpret_cast<std::uintptr_t>(values.data()),
                reinterpret_cast<std::uintptr_t>(values.data() + values.size()),
                reinterpret_cast<std::uintptr_t>(values.data() + values.size())
            };
            std::memcpy(views[i].bytes.data() + 576, ids.data(), sizeof(ids));
        }
        header = {
            reinterpret_cast<std::uintptr_t>(views.data()),
            reinterpret_cast<std::uintptr_t>(views.data() + views.size()),
            reinterpret_cast<std::uintptr_t>(views.data() + views.size())
        };
        std::memcpy(context.borrowed.data() + 16, header.data(), sizeof(header));
    }
};

LL_STATIC_HOOK(
    ProjectionChunkGeometryHook,
    ll::memory::HookPriority::Normal,
    chunkEntry,
    void,
    void*                                 self,
    void const*                           context,
    NativeChunkGeometryDescription const& description
) {
    std::uint32_t lighting;
    std::memcpy(&lighting, static_cast<std::byte const*>(context) + 112, sizeof(lighting));
    auto const* layer = description.mTerrainLayer;
    if (description.mIsHighlighted && lighting == 1 && layer) {


        alignas(NativeChunkGeometryDescription) std::array<std::byte, sizeof(NativeChunkGeometryDescription)> borrowed;
        std::memcpy(borrowed.data(), &description, borrowed.size());
        borrowed[offsetof(NativeChunkGeometryDescription, mIsHighlighted)] = std::byte{0};


        std::vector<NativePerChunkData> chunks(description.mChunks.begin(), description.mChunks.end());
        for (auto& chunk : chunks) chunk.mVisibleCameraMask = 1;
        gsl::span<NativePerChunkData const> visibleChunks{chunks};
        std::memcpy(
            borrowed.data() + offsetof(NativeChunkGeometryDescription, mChunks),
            &visibleChunks,
            sizeof(visibleChunks)
        );
        auto const& prepass = *reinterpret_cast<NativeChunkGeometryDescription const*>(borrowed.data());
        submitBlockEntityPrepass([&] {
            origin(self, context, prepass);
            return std::size_t{1};
        });
    }
    origin(self, context, description);
}


using SelectionDrawFn = void (*)(
    void*               declarator,
    void const*         context,
    int                 sortMode,
    HashedString const& pass,
    std::string const&  name,
    bool                sorted,
    void const*         color,
    void const*         depth
);

void drawHighlightMarkers(void* declarator, void const* context, void const* color, void const* depth) {
    if (!selectionTransparentEntry) return;
    static HashedString const transparent{"Transparent"};
    for (auto view : {detail::validationEdgeView, detail::validationFaceView}) {
        ProjectionPassContext markers(
            NativePassContext(context, NativePassState{true, false, NativeDepthComparison::LessEqual}),
            [view](auto) { return std::vector<std::uint16_t>{view}; }
        );
        reinterpret_cast<SelectionDrawFn>(selectionTransparentEntry)(
            declarator,
            &markers.context,
            2,
            transparent,
            view == detail::validationEdgeView ? "LeviSchematicValidationEdges" : "LeviSchematicValidationFaces",
            true,
            color,
            depth
        );
    }
}

void drawBlendDepth(
    void*              declarator,
    void const*        context,
    std::string const& name,
    void const*        color,
    void const*        optionalColor,
    void const*        depth,
    void const*        buffer1,
    void const*        buffer2,
    void const*        texture1,
    void const*        texture2,
    void const*        texture3,
    void const*        texture4,
    void const*        texture5,
    void const*        texture6
);

void finishHighlightDeclaration(
    void*       declarator,
    void const* context,
    int         sortMode,
    void const* color,
    void const* depth
) {
    if (!markersPending || sortMode != 1) return;
    markersPending = false;
    drawHighlightMarkers(declarator, context, color, depth);
}

LL_STATIC_HOOK(
    ProjectionAlphaGeometryHook,
    ll::memory::HookPriority::Normal,
    alphaEntry,
    void,
    void*               declarator,
    void const*         context,
    int                 sortMode,
    HashedString const& pass,
    std::string const&  name,
    void const*         color,
    void const*         optionalColor,
    void const*         depth,
    void const*         buffer1,
    void const*         buffer2,
    void const*         texture1,
    void const*         texture2,
    void const*         texture3,
    void const*         texture4,
    void const*         texture5,
    void const*         texture6
) {
    if (pass.getString() != "ForwardPBRAlphaTest" || !coverageTarget) {
        origin(
            declarator,
            context,
            sortMode,
            pass,
            name,
            color,
            optionalColor,
            depth,
            buffer1,
            buffer2,
            texture1,
            texture2,
            texture3,
            texture4,
            texture5,
            texture6
        );
        if (markersPending && sortMode == 1 && !coverageTarget)
            drawBlendDepth(
                declarator,
                context,
                name,
                color,
                optionalColor,
                depth,
                buffer1,
                buffer2,
                texture1,
                texture2,
                texture3,
                texture4,
                texture5,
                texture6
            );
        finishHighlightDeclaration(declarator, context, sortMode, color, depth);
        return;
    }
    if (sortMode == 0) {
        static HashedString const geometry{"GeometryPrepassAlphaTest"};
        ProjectionPassContext     coverage(context, true);
        NativeColorBinding        coverageColor;
        std::memcpy(&coverageColor, color, sizeof(coverageColor));
        coverageColor.resource = coverageTarget;
        origin(
            declarator,
            &coverage.context,
            0,
            geometry,
            name + "/ProjectionCoverage",
            &coverageColor,
            optionalColor,
            depth,
            buffer1,
            buffer2,
            texture1,
            texture2,
            texture3,
            texture4,
            texture5,
            texture6
        );
    }
    ProjectionPassContext forward(context, false);
    origin(
        declarator,
        &forward.context,
        sortMode,
        pass,
        name,
        color,
        optionalColor,
        depth,
        buffer1,
        buffer2,
        texture1,
        texture2,
        texture3,
        texture4,
        texture5,
        texture6
    );
    finishHighlightDeclaration(declarator, context, sortMode, color, depth);
}


LL_STATIC_HOOK(
    ProjectionOpaqueGeometryHook,
    ll::memory::HookPriority::Normal,
    opaqueEntry,
    void,
    void*               declarator,
    void const*         context,
    int                 sortMode,
    HashedString const& pass,
    std::string const&  name,
    void const*         color,
    void const*         optionalColor,
    void const*         depth,
    void const*         buffer1,
    void const*         buffer2,
    void const*         texture1,
    void const*         texture2,
    void const*         texture3,
    void const*         texture4,
    void const*         texture5,
    void const*         texture6
) {
    if (pass.getString() != "ForwardPBROpaque" || !coverageTarget) {
        std::optional<ProjectionPassContext> filtered;
        if (inHighlightEffect && !coverageTarget)
            filtered.emplace(NativePassContext(context), [](auto original) {
                return detail::projectionColorPassViews(original);
            });
        origin(
            declarator,
            filtered ? &filtered->context : context,
            sortMode,
            pass,
            name,
            color,
            optionalColor,
            depth,
            buffer1,
            buffer2,
            texture1,
            texture2,
            texture3,
            texture4,
            texture5,
            texture6
        );
        return;
    }
    if (sortMode == 0) {
        static HashedString const geometry{"GeometryPrepass"};
        ProjectionPassContext     coverage(context, true);
        NativeColorBinding        coverageColor;
        std::memcpy(&coverageColor, color, sizeof(coverageColor));
        coverageColor.resource = coverageTarget;
        origin(
            declarator,
            &coverage.context,
            0,
            geometry,
            name + "/ProjectionCoverage",
            &coverageColor,
            optionalColor,
            depth,
            buffer1,
            buffer2,
            texture1,
            texture2,
            texture3,
            texture4,
            texture5,
            texture6
        );
    }
    ProjectionPassContext forward(context, false);
    origin(
        declarator,
        &forward.context,
        sortMode,
        pass,
        name,
        color,
        optionalColor,
        depth,
        buffer1,
        buffer2,
        texture1,
        texture2,
        texture3,
        texture4,
        texture5,
        texture6
    );
}


void drawBlendDepth(
    void*              declarator,
    void const*        context,
    std::string const& name,
    void const*        color,
    void const*        optionalColor,
    void const*        depth,
    void const*        buffer1,
    void const*        buffer2,
    void const*        texture1,
    void const*        texture2,
    void const*        texture3,
    void const*        texture4,
    void const*        texture5,
    void const*        texture6
) {
    static HashedString const opaque{"Opaque"};
    ProjectionPassContext     blendDepth(
        NativePassContext(context, NativePassState{false, true, NativeDepthComparison::LessEqual}),
        [](auto) { return std::vector<std::uint16_t>{detail::projectionBlendDepthView}; }
    );
    ProjectionOpaqueGeometryHook::origin(
        declarator,
        &blendDepth.context,
        0,
        opaque,
        name + "/ProjectionBlendDepth",
        color,
        optionalColor,
        depth,
        buffer1,
        buffer2,
        texture1,
        texture2,
        texture3,
        texture4,
        texture5,
        texture6
    );
}

LL_STATIC_HOOK(
    ProjectionHighlightImageHook,
    ll::memory::HookPriority::Normal,
    imageEntry,
    void const*,
    void*                         declarator,
    void*                         constructorContext,
    NativeImageDescription const& description
) {
    if (!promoteHighlightTarget || description.format != mce::TextureFormat::R8g8b8a8Unorm)
        return origin(declarator, constructorContext, description);

    promoteHighlightTarget = false;
    auto hdr               = description;
    hdr.format             = mce::TextureFormat::R16g16b16a16Float;
    auto const* result     = origin(declarator, constructorContext, hdr);


    auto coverage   = description;
    coverage.format = mce::TextureFormat::R32g32b32a32Uint;
    coverageTarget  = origin(declarator, constructorContext, coverage);
    return result;
}


LL_STATIC_HOOK(
    ProjectionHighlightEffectHook,
    ll::memory::HookPriority::Normal,
    highlightEntry,
    void,
    void*       declarator,
    void*       allocator,
    void const* frame,
    void const* overrides,
    void const* mesh,
    void const* viewport,
    void const* resources,
    void const* cameras,
    void*       color,
    void*       depth,
    void const* scissor,
    int         sceneMode,
    bool        preExposure,
    void const* buffer1,
    void const* buffer2,
    void const* texture1,
    void const* texture2,
    void const* texture3,
    void const* texture4,
    void const* texture5,
    void const* optionalColor
) {
    bool const  previous         = promoteHighlightTarget;
    auto const* previousCoverage = coverageTarget;
    bool const  previousPending  = markersPending;
    bool const  previousEffect   = inHighlightEffect;
    inHighlightEffect            = true;
    promoteHighlightTarget       = preExposure;
    coverageTarget               = nullptr;
    markersPending               = true;
    struct Restore {
        bool        previous;
        void const* previousCoverage;
        bool        previousPending;
        bool        previousEffect;
        ~Restore() {
            inHighlightEffect      = previousEffect;
            promoteHighlightTarget = previous;
            coverageTarget         = previousCoverage;
            markersPending         = previousPending;
        }
    } restore{previous, previousCoverage, previousPending, previousEffect};
    origin(
        declarator,
        allocator,
        frame,
        overrides,
        mesh,
        viewport,
        resources,
        cameras,
        color,
        depth,
        scissor,
        sceneMode,
        preExposure,
        buffer1,
        buffer2,
        texture1,
        texture2,
        texture3,
        texture4,
        texture5,
        optionalColor
    );
}
}

bool registerProjectionHighlightPipeline() {
    if (installed) return true;
    highlightEntry = resolveProjectionEntry(
        "",
        "55 41 57 41 56 41 55 41 54 56 57 53 B8 98 15 00 00 E8 ? ? ? ? 48 29 C4 "
        "48 8D AC 24 ? ? ? ? 44 0F 29 8D"
    );
    imageEntry = resolveProjectionEntry(
        "",
        "55 41 56 56 57 53 48 83 EC 50 48 8D 6C 24 ? 48 C7 45 ? ? ? ? ? 4C 89 C3 "
        "48 89 CE 48 8B 0D ? ? ? ? 48 8B 01 48 8B 40 ? BA 28 00 00 00"
    );
    chunkEntry = resolveProjectionEntry("", "55 41 57 41 56 41 55 41 54 56 57 53 B8 08 18 00 00");
    alphaEntry = resolveProjectionEntry(
        "",
        "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 98 0C 00 00 48 8D AC 24 80 00 00 00 "
        "0F 29 B5 00 0C 00 00 48 C7 85 F8 0B 00 00 FE FF FF FF 44 89 85 AC 0B 00 00 "
        "48 89 8D 28 0B 00 00 49 83 C1 08 4C 89 8D 48 0B 00 00 0F 57 F6 4C 8B 6A 10 "
        "48 8B 42 18 48 89 85 40 0B 00 00 48 8D 42 28 48 89 85 30 0B 00 00 48 89 95 C0 0B 00 00 "
        "48 8D 42 60 48 89 85 38 0B 00 00 EB 10 66 0F 1F 84 00 00 00 00 00 "
        "49 81 C5 90 02 00 00 4C 3B AD 40 0B 00 00 0F 84 9C 1D 00 00 49 8D 85 40 02 00 00 "
        "48 8B 8D C0 0B 00 00 48 8B 71 08 48 8D 0D 23 D3 80 03"
    );
    opaqueEntry = resolveProjectionEntry(
        "",
        "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 98 0C 00 00 48 8D AC 24 80 00 00 00 "
        "0F 29 B5 00 0C 00 00 48 C7 85 F8 0B 00 00 FE FF FF FF 44 89 85 AC 0B 00 00 "
        "48 89 8D 28 0B 00 00 49 83 C1 08 4C 89 8D 48 0B 00 00 0F 57 F6 4C 8B 6A 10 "
        "48 8B 42 18 48 89 85 40 0B 00 00 48 8D 42 28 48 89 85 30 0B 00 00 48 89 95 C0 0B 00 00 "
        "48 8D 42 60 48 89 85 38 0B 00 00 EB 10 66 0F 1F 84 00 00 00 00 00 "
        "49 81 C5 90 02 00 00 4C 3B AD 40 0B 00 00 0F 84 9C 1D 00 00 49 8D 85 40 02 00 00 "
        "48 8B 8D C0 0B 00 00 48 8B 71 08 48 8D 0D A3 F5 80 03"
    );
    selectionTransparentEntry = resolveProjectionEntry(
        "",
        "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 18 0A 00 00 48 8D AC 24 ? ? ? ? 0F 29 BD ? ? ? ? "
        "0F 29 B5 ? ? ? ? 48 C7 85 ? ? ? ? ? ? ? ? 48 89 8D ? ? ? ? 4C 8B A5 ? ? ? ? 4C 8B 72 ? 48 8B 42 ? "
        "48 89 85 ? ? ? ? 48 8D 42 ? 48 89 85 ? ? ? ? 4C 8D BD ? ? ? ? 49 83 C1 08 4C 89 8D ? ? ? ? 0F 57 F6 "
        "48 89 95 ? ? ? ? 48 8D 42 ? 48 89 85 ? ? ? ? 44 89 85 ? ? ? ? EB ? 48 83 C0 28 48 89 C2 4C 89 C1 "
        "E8 ? ? ? ? 49 81 C6 90 02 00 00 4C 3B B5 ? ? ? ? 0F 84 ? ? ? ? 49 8D B6 ? ? ? ? 80 BD ? ? ? ? ? "
        "0F 84 ? ? ? ? 48 8B 85 ? ? ? ? 48 8B 78 ? 48 8D 05 ? ? ? ? 48 89 85 ? ? ? ? 48 89 B5 ? ? ? ? "
        "48 8D 85 ? ? ? ? 48 89 85 ? ? ? ? 48 8B 9F ? ? ? ? 48 81 C7 10 0A 00 00 48 39 FB 75 ? 31 FF "
        "48 8D 05 ? ? ? ? 48 8D 95 ? ? ? ? 48 89 D1 4C 8D BD ? ? ? ? 4C 8B A5 ? ? ? ? E9 ? ? ? ? "
        "66 0F 1F 44 00 ? 48 8B 1B 48 39 FB 0F 84 ? ? ? ? 4C 8B 7B ? 4C 8B 63 ? 4D 39 E7 74 ? 4C 8D 6B ? "
        "EB ? 90 49 81 C7 C8 00 00 00 4D 39 E7 74 ? 41 F6 07 20 74 ? 49 8B 47 ? 83 E0 40"
    );
    bool const imageHooked  = imageEntry && ProjectionHighlightImageHook::hook() == 0;
    bool const effectHooked = highlightEntry && ProjectionHighlightEffectHook::hook() == 0;
    bool const chunkHooked  = chunkEntry && ProjectionChunkGeometryHook::hook() == 0;
    bool const alphaHooked  = alphaEntry && ProjectionAlphaGeometryHook::hook() == 0;
    bool const opaqueHooked = opaqueEntry && ProjectionOpaqueGeometryHook::hook() == 0;
    installed               = imageHooked && effectHooked && chunkHooked && alphaHooked && opaqueHooked;
    if (!installed) {
        if (opaqueHooked) ProjectionOpaqueGeometryHook::unhook();
        if (alphaHooked) ProjectionAlphaGeometryHook::unhook();
        if (chunkHooked) ProjectionChunkGeometryHook::unhook();
        if (effectHooked) ProjectionHighlightEffectHook::unhook();
        if (imageHooked) ProjectionHighlightImageHook::unhook();
    }
    return installed;
}

void unregisterProjectionHighlightPipeline() {
    if (installed) {
        ProjectionOpaqueGeometryHook::unhook();
        ProjectionAlphaGeometryHook::unhook();
        ProjectionChunkGeometryHook::unhook();
        ProjectionHighlightEffectHook::unhook();
        ProjectionHighlightImageHook::unhook();
    }
    installed      = false;
    highlightEntry = imageEntry = 0;
    chunkEntry = alphaEntry = opaqueEntry = 0;
    selectionTransparentEntry             = 0;
}

bool validationMarkerHighlightAvailable() { return installed && selectionTransparentEntry != 0; }

bool projectionBlendDepthAvailable() { return installed; }
}
