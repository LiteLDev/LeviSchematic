#include "BlockEntityRenderDiagnostics.h"
#include "levischematic/LeviSchematic.h"

#include "ll/api/memory/Hook.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/model/models/Model.h"
#include "mc/client/model/models/SkinnedMesh.h"
#include "mc/client/model/models/SkinnedMeshGroup.h"
#include "mc/client/renderer/blockactor/ChestRenderer.h"
#include "mc/deps/minecraft_renderer/resources/MERSUniformData.h"
#include "mc/deps/renderer/Camera.h"

namespace levischematic::render::diagnostics {
namespace {
thread_local BlockEntityTrace* activeTrace = nullptr;
bool                           registered  = false;


LL_TYPE_INSTANCE_HOOK(
    ChestTraceHook,
    HookPriority::Normal,
    ChestRenderer,
    &ChestRenderer::$render,
    void,
    BaseActorRenderContext& context,
    BlockActorRenderData&   data
) {
    if (activeTrace) ++activeTrace->chestCalls;
    origin(context, data);
}

using ModelRenderFunction = void (BlockActorRenderer::*)(
    ScreenContext&,
    dragon::RenderMetadata const&,
    Model&,
    ActorTextureInfo const&,
    mce::MaterialPtr const&,
    mce::ClientTexture const&
) const;


LL_TYPE_INSTANCE_HOOK(
    ModelTraceHook,
    HookPriority::Normal,
    BlockActorRenderer,
    static_cast<ModelRenderFunction>(&BlockActorRenderer::_renderModel),
    void,
    ScreenContext&                context,
    dragon::RenderMetadata const& metadata,
    Model&                        model,
    ActorTextureInfo const&       texture,
    mce::MaterialPtr const&       material,
    mce::ClientTexture const&     forcedTexture
) {
    if (activeTrace) {
        ++activeTrace->modelCalls;
        activeTrace->modelGeometries  += model.mGeometries->size();
        auto const& translation        = context.camera.worldMatrixStack->stack->back()._m.get()[3];
        activeTrace->modelTranslation  = {translation.x, translation.y, translation.z};
    }
    origin(context, metadata, model, texture, material, forcedTexture);
}


LL_TYPE_INSTANCE_HOOK(
    GroupTraceHook,
    HookPriority::Normal,
    SkinnedMeshGroup,
    &SkinnedMeshGroup::draw,
    void,
    ScreenContext&                        context,
    dragon::RenderMetadata const&         metadata,
    MaterialVariants const&               materials,
    ClientPBRTextureData const&           textures,
    std::optional<MERSUniformData> const& mers
) {
    if (activeTrace) {
        ++activeTrace->groupCalls;
        if (!mShouldDrawThisFrame) ++activeTrace->hiddenGroups;
    }
    origin(context, metadata, materials, textures, mers);
}


LL_TYPE_INSTANCE_HOOK(
    MeshTraceHook,
    HookPriority::Normal,
    SkinnedMesh,
    &SkinnedMesh::draw,
    void,
    ScreenContext&                        context,
    dragon::RenderMetadata const&         metadata,
    RenderController const*               controller,
    uint                                  subLayer,
    MaterialVariants const&               materials,
    ClientPBRTextureData const&           textures,
    SortedMeshDrawList*                   sorted,
    std::optional<glm::mat4x4> const&     previous,
    std::optional<MERSUniformData> const& mers,
    ShadowContext                         shadow,
    bool                                  dithering
) {
    if (activeTrace) {
        ++activeTrace->meshCalls;
        if (!mShouldDrawThisFrame) ++activeTrace->hiddenMeshes;
        if (mVertexCount > 0) activeTrace->indexCountOverride += static_cast<std::size_t>(mVertexCount);
        for (auto const* mesh : {&mSkinnedMeshQuads.get(), &mSkinnedMeshTris.get()}) {
            if (mesh->isValid()) ++activeTrace->validMeshes;
            activeTrace->meshVertices += mesh->mVertexCount->has_value()
                                           ? static_cast<std::size_t>(mesh->mVertexCount->value())
                                           : mesh->mMeshData->mPositions->size();
            activeTrace->cpuIndices   += mesh->mMeshData->mIndices->size();
        }
    }
    origin(context, metadata, controller, subLayer, materials, textures, sorted, previous, mers, shadow, dithering);
}

} 

BlockEntityTraceScope::BlockEntityTraceScope(BlockEntityTrace* trace) noexcept : mPrevious(activeTrace) {
    activeTrace = trace;
}

BlockEntityTraceScope::~BlockEntityTraceScope() { activeTrace = mPrevious; }

void registerHooks() {
    if (registered) return;
    auto const chest = ChestTraceHook::hook();
    auto const model = ModelTraceHook::hook();
    auto const group = GroupTraceHook::hook();
    auto const mesh  = MeshTraceHook::hook();
    LeviSchematic::getInstance().getSelf().getLogger().info(
        "SchematicBlockEntities: hookResults chest={} model={} group={} mesh={} (raw LeviLamina results)",
        chest,
        model,
        group,
        mesh
    );
    registered = true;
}

void unregisterHooks() {
    if (!registered) return;
    MeshTraceHook::unhook();
    GroupTraceHook::unhook();
    ModelTraceHook::unhook();
    ChestTraceHook::unhook();
    registered = false;
}

} 
