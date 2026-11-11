#include "ActorProjection.h"
#include "BlockEntityHighlight.h"
#include "EntityVisualState.h"
#include "SchematicChunkRenderer.h"
#include "levischematic/app/AppKernel.h"
#include "levischematic/schematic/FakeWorld.h"
#include "levischematic/schematic/placement/PlacementGeometry.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/actor/ActorRenderDispatcher.h"
#include "mc/client/renderer/actor/ActorRenderer.h"
#include "mc/client/renderer/actor/v2/AnimationRemappingInfo.h"
#include "mc/client/renderer/actor/v2/DataDrivenRendererContinuousData.h"
#include "mc/client/renderer/actor/v2/DataDrivenRendererEntry.h"
#include "mc/client/renderer/actor/v2/DataDrivenRendererV2.h"
#include "mc/client/renderer/actor/v2/GameplayRelatedRenderData.h"
#include "mc/client/renderer/actor/v2/RequestedDataDrivenRender.h"
#include "mc/dataloadhelper/StructureDataLoadHelper.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/FloatTag.h"
#include "mc/deps/nbt/ByteTag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/StringTag.h"
#include "mc/entity/components/DimensionTypeComponent.h"
#include "mc/entity/components/RenderPositionComponent.h"
#include "mc/entity/components/RenderRotationComponent.h"
#include "mc/entity/components/ActorRotationComponent.h"
#include "mc/deps/vanilla_components/StateVectorComponent.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDefinitionIdentifier.h"
#include "mc/world/actor/ActorFactory.h"
#include "mc/world/actor/ActorCategory.h"
#include "mc/world/actor/ActorDataIDs.h"
#include "mc/world/actor/SynchedActorDataEntityWrapper.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/actor/item/Minecart.h"
#include "mc/entity/utilities/RailMovementUtility.h"
#include "mc/world/level/ActorGarbageCollector.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include <cmath>
#include <map>
#include <set>

namespace levischematic::render {
namespace {
using AddActor          = bool (*)(DataDrivenRendererV2*, Actor&, std::shared_ptr<ActorRenderer> const&, ShadowContext);
AddActor       addActor = nullptr;
std::uintptr_t generateEntry = 0;
std::uintptr_t railPositionEntry = 0, railOffsetEntry = 0;
bool           installed     = false;
struct ProjectionRailContext {
    IConstBlockSource const* source;
    Vec3 origin;
};
thread_local ProjectionRailContext const* projectionRails = nullptr;
thread_local bool insideRailQuery = false;
class RailScope {
    ProjectionRailContext const* previous = projectionRails;
    ProjectionRailContext context;
public:
    RailScope(BlockSource const& source, BlockPos const& origin) : context{&source, Vec3{origin}} {
        projectionRails = &context;
    }
    ~RailScope() { projectionRails = previous; }
};


LL_STATIC_HOOK(ProjectionRailPositionHook, ll::memory::HookPriority::Normal,
    railPositionEntry, bool, IConstBlockSource const& source, Vec3& result, Vec3 position) {
    if (!projectionRails || insideRailQuery) return origin(source, result, position);
    insideRailQuery = true;
    struct Restore { ~Restore() { insideRailQuery = false; } } restore;
    bool found = origin(*projectionRails->source, result, position - projectionRails->origin);
    if (found) result += projectionRails->origin;
    return found;
}
LL_STATIC_HOOK(ProjectionRailOffsetHook, ll::memory::HookPriority::Normal,
    railOffsetEntry, bool, IConstBlockSource const& source, Vec3& result, Vec3 position, float offset) {
    if (!projectionRails || insideRailQuery) return origin(source, result, position, offset);
    insideRailQuery = true;
    struct Restore { ~Restore() { insideRailQuery = false; } } restore;
    bool found = origin(*projectionRails->source, result, position - projectionRails->origin, offset);
    if (found) result += projectionRails->origin;
    return found;
}
template <class T>
class SwapScope {
    T& a;
    T& b;

public:
    SwapScope(T& left, T& right) : a(left), b(right) { a.swap(b); }
    ~SwapScope() { a.swap(b); }
};



struct History {
    std::vector<DataDrivenRendererEntry>                   entries;
    std::vector<uint64>                                    ids;
    std::vector<glm::mat4x4>                               transforms;
    std::vector<AnimationRemappingInfo>                    animation;
    std::vector<PreviousTransformationData::BoneRemapping> remapping;
    std::vector<glm::mat4x4>                               bones;
};
struct DisplayActor {
    OwnerPtr<EntityContext> owner;
    ActorGarbageCollector&  collector;
    DisplayActor(OwnerPtr<EntityContext>&& value, ActorGarbageCollector& gc) : owner(std::move(value)), collector(gc) {
        value.reset();
    }
    ~DisplayActor() {
        
        
        
        
        if (owner) {
            collector.garbageCollectEntity(std::move(owner));
            owner.reset();
        }
    }
    Actor* get() const {
        auto value = owner.tryUnwrap();
        return value ? &*value : nullptr;
    }
};
LL_TYPE_INSTANCE_HOOK(
    ProjectionActorFrameHook,
    ll::memory::HookPriority::Normal,
    DataDrivenRendererV2,
    generateEntry,
    uint,
    GameplayRelatedRenderData const& data
) {
    auto result  = origin(data);
    auto session = app::tryAccessSession();
    if (session && session->hudReady() && session->schematicRenderer().prepareEntityComposite())
        session->actors().draw(*this, data, session->placement(), session->schematicRenderer(), [&] { (void)origin(data); });
    return result;
}
} 
struct ActorProjection::Impl {
    struct Entry {
        uint64_t                                   revision = 0;
        size_t                                     cursor   = 0;
        std::vector<std::unique_ptr<DisplayActor>> actors;
    };
    std::map<placement::PlacementId, Entry> entries;
    History                                 history;
    ActorProjectionStats                    stats;
    int                                     dimension = INT32_MIN;
};
ActorProjection::ActorProjection() : mImpl(std::make_unique<Impl>()) {}
ActorProjection::~ActorProjection() = default;
void                 ActorProjection::clear() { mImpl = std::make_unique<Impl>(); }
ActorProjectionStats ActorProjection::stats() const { return mImpl->stats; }
void                 ActorProjection::tick(
    ClientInstance&              client,
    app::PlacementService const& placements,
    util::WorkBudget const&      budget
) {
    auto player = client.getLocalPlayer();
    auto level  = client.getLevel();
    if (!player || !level) {
        clear();
        return;
    }
    if (!installed) {
        mImpl->stats.detail = "Native entity projection bridge unavailable (requires verified 1.26.60.5 entries)";
        return;
    }
    int dimension = static_cast<int>(player->getDimensionId());
    if (mImpl->dimension != dimension) {
        clear();
        mImpl->dimension = dimension;
    }
    std::set<placement::PlacementId> live;
    for (auto const& ref : placements.orderedPlacements()) {
        auto const& p = ref.get();
        if (!p.enabled || !p.renderEnabled || !p.entitiesEnabled || p.dimensionId != dimension || !p.asset
            || !p.asset->structure)
            continue;
        auto distance = app::configStore().snapshot().visibility.renderDistanceBlocks;
        if (distance) {
            auto size = p.asset->size;
            if (static_cast<int>(p.rotation) % 2) std::swap(size.x, size.z);
            auto   pos = player->getPosition();
            double dx  = std::max({double(p.origin.x) - pos.x, 0.0, double(pos.x) - p.origin.x - size.x});
            double dy  = std::max({double(p.origin.y) - pos.y, 0.0, double(pos.y) - p.origin.y - size.y});
            double dz  = std::max({double(p.origin.z) - pos.z, 0.0, double(pos.z) - p.origin.z - size.z});
            if (dx * dx + dy * dy + dz * dz > double(distance + 32) * (distance + 32)) continue;
        }
        live.insert(p.id);
        auto& entry = mImpl->entries[p.id];
        if (entry.revision != p.revision) {
            entry          = {};
            entry.revision = p.revision;
        }
        auto const& data     = p.asset->structure->mStructureTemplateData.get();
        auto const& entities = data.mEntityData.get();
        while (entry.cursor < entities.size() && budget.available()) {
            auto const& original = entities[entry.cursor++];
            try {
                auto                    tag = original->clone();
                StructureDataLoadHelper helper;
                helper.mStructurePlacementLocation = p.origin;
                helper.mStructureWorldOrigin       = data.mStructureWorldOrigin;
                auto size                          = p.asset->size;
                helper.mPivot                      = Vec3{(size.x - 1) * .5f, (size.y - 1) * .5f, (size.z - 1) * .5f};
                helper.mOwner                      = ActorUniqueID::INVALID_ID();
                helper.mRotation                   = toMinecraftRotation(p.rotation);
                helper.mMirror                     = toMinecraftMirror(p.mirror);
                helper.mLevel                      = level;
                std::string identifier             = tag->at("identifier").get<StringTag>();
                auto const& position               = tag->at("Pos").get<ListTag>();
                auto const& rotation               = tag->at("Rotation").get<ListTag>();
                if (position.size() != 3 || rotation.size() != 2)
                    throw std::runtime_error("Invalid entity position/rotation NBT.");
                Vec3 source{
                    position[0].get<FloatTag>().data,
                    position[1].get<FloatTag>().data,
                    position[2].get<FloatTag>().data
                };
                auto pos = helper.loadPosition(source);
                Vec2 rot{
                    helper.loadRotationDegreesX(rotation[1].get<FloatTag>().data),
                    helper.loadRotationDegreesY(rotation[0].get<FloatTag>().data)
                };
                if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z) || !std::isfinite(rot.x)
                    || !std::isfinite(rot.y) || double(pos.x) < INT32_MIN || double(pos.x) > INT32_MAX
                    || double(pos.y) < INT32_MIN || double(pos.y) > INT32_MAX || double(pos.z) < INT32_MIN
                    || double(pos.z) > INT32_MAX)
                    throw std::runtime_error("Invalid transformed entity coordinates.");
                auto owner = level->getActorFactory()
                                 .clientCreateDisplayActor(ActorDefinitionIdentifier{identifier}, nullptr, pos, rot);
                if (!owner) {
                    ++mImpl->stats.skipped;
                    mImpl->stats.detail = "Client display factory rejected " + identifier;
                    continue;
                }
                auto  holder = std::make_unique<DisplayActor>(std::move(owner), *level->mActorGarbageCollector);
                auto* actor  = holder->get();
                if (!actor) {
                    ++mImpl->stats.skipped;
                    continue;
                }
                
                
                
                actor->mLevel = level;
                applyEntityVisualState(*actor, *tag);
                if (!holder->owner->hasComponent<DimensionTypeComponent>())
                    (void)holder->owner->getOrAddComponent<DimensionTypeComponent>();
                actor->setDimension(WeakRef<Dimension>{player->getDimension().shared_from_this()});
                
                
                
                
                if (identifier == "minecraft:painting") actor->readAdditionalSaveData(*tag, helper);
                if (actor->hasCategory(ActorCategory::Minecart)) {
                    auto* minecart = static_cast<Minecart*>(actor);
                    
                    
                    
                    if (tag->contains("CustomDisplayTile", Tag::Type::Byte)
                        && tag->at("CustomDisplayTile").get<ByteTag>().data) {
                        std::vector<std::unique_ptr<DataItem>> display;
                        if (tag->contains("DisplayBlock", Tag::Type::Compound)) {
                            minecart->mDisplayBlockSerId = tag->at("DisplayBlock").get<CompoundTag>().clone();
                            minecart->_lazyInitDisplayBlock();
                        } else if (tag->contains("DisplayTile", Tag::Type::Int)
                            && tag->contains("DisplayData", Tag::Type::Int)) {
                            auto block = Block::tryGetFromRegistry(
                                static_cast<uint>(tag->at("DisplayTile").get<IntTag>().data),
                                static_cast<ushort>(tag->at("DisplayData").get<IntTag>().data));
                            if (block) {
                                display.push_back(DataItem::create(ActorDataIDs::DisplayTileRuntimeId,
                                    static_cast<int>(block->mNetworkId)));
                                display.push_back(DataItem::create(ActorDataIDs::CustomDisplay, static_cast<uchar>(1)));
                            }
                        }
                        if (tag->contains("DisplayOffset", Tag::Type::Int))
                            display.push_back(DataItem::create(ActorDataIDs::DisplayOffset,
                                tag->at("DisplayOffset").get<IntTag>().data));
                        if (!display.empty()) minecart->mEntityData->assignValues(display, *minecart, std::nullopt);
                    }
                    minecart->mRailRenderRot = rot.y;
                }
                
                
                
                auto& state = *actor->mBuiltInComponents->mStateVectorComponent;
                state.mPosPrev = state.mPos;
                actor->mBuiltInComponents->mActorRotationComponent->mRotPrev = actor->getRotation();
                actor->getEntityContext().getOrAddComponent<RenderPositionComponent>().mValue = actor->getPosition();
                actor->getEntityContext().getOrAddComponent<RenderRotationComponent>().mRot = actor->getRotation();
                (void)actor->getOrCreateUniqueID();
                entry.actors.push_back(std::move(holder));
            } catch (std::exception const& e) {
                ++mImpl->stats.skipped;
                mImpl->stats.detail = e.what();
            }
        }
    }
    std::erase_if(mImpl->entries, [&](auto const& item) { return !live.contains(item.first); });
    mImpl->stats.loaded = 0;
    for (auto const& [id, entry] : mImpl->entries) mImpl->stats.loaded += entry.actors.size();
}
void ActorProjection::draw(
    DataDrivenRendererV2&            renderer,
    GameplayRelatedRenderData const& data,
    app::PlacementService const&     placements,
    SchematicChunkRenderer&          chunks,
    std::function<void()> const&     generate
) {
    mImpl->stats.submitted = mImpl->stats.highlighted = 0;
    mImpl->stats.submittedByType.clear();
    mImpl->stats.missingRendererByType.clear();
    mImpl->stats.legacyByType.clear();
    if (mImpl->stats.detail == "Actor renderer unavailable") mImpl->stats.detail.clear();
    if (!addActor || !data.mContext || !data.mContext->mV2DataDrivenContinuousData) return;
    auto dispatcher = data.mContext->mEntityRenderDispatcher;
    if (!dispatcher) return;
    std::vector<RequestedDataDrivenRender> requests;
    std::vector<Actor*>                    fire, leashes;
    SwapScope                              requestScope(renderer.mRequestedActors.get(), requests);
    SwapScope                              fireScope(renderer.mOnFireActors.get(), fire);
    SwapScope                              leashScope(renderer.mLeashedActors.get(), leashes);
    auto& native             = data.mContext->mV2DataDrivenContinuousData->mPreviousTransformationData.get();
    auto& screen = data.mContext->mScreenContext;
    bool const previousHighlight = screen.isDrawingEditorSelectionHighlight;
    bool const previousDithering = screen.isDitheringEnabled;
    screen.isDrawingEditorSelectionHighlight = true;
    screen.isDitheringEnabled = false;
    struct RestoreFlags {
        ScreenContext& screen;
        bool highlight, dithering;
        ~RestoreFlags() {
            screen.isDrawingEditorSelectionHighlight = highlight;
            screen.isDitheringEnabled = dithering;
        }
    } restoreFlags{screen, previousHighlight, previousDithering};
    std::optional<int64> uniqueIdOverride;
    SwapScope uniqueIdScope(data.mContext->mRenderUniqueIdOverride.get(), uniqueIdOverride);
    auto  stats              = submitBlockEntityHighlight([&] {
        size_t count = 0;
        for (auto const& [id, entry] : mImpl->entries) {
            auto p = placements.findPlacement(id);
            if (!p || !p->enabled || !p->renderEnabled || !p->entitiesEnabled || p->revision != entry.revision)
                continue;
            for (auto const& holder : entry.actors) {
                auto actor = holder->get();
                if (!actor) continue;
                auto pos = actor->getPosition();
                if (!placements.viewState().layerRange.containsPosition(BlockPos{pos})) continue;
                auto config   = app::configStore().snapshot();
                auto delta    = pos - data.mCameraTargetPos.get();
                auto distance = config.visibility.renderDistanceBlocks;
                if (distance && delta.x * delta.x + delta.y * delta.y + delta.z * delta.z > float(distance) * distance)
                    continue;
                
                
                
                auto model = dispatcher->getRenderer(actor->getActorRendererId());
                if (!model) model = dispatcher->getRenderer(actor->queryEntityRenderer());
                auto const& identifier = actor->getActorIdentifier().mCanonicalName->getString();
                if (!model) {
                    mImpl->stats.detail = "Actor renderer unavailable";
                    ++mImpl->stats.missingRendererByType[identifier];
                }
                else if (addActor(&renderer, *actor, model, ShadowContext::FullDraw)) {
                    
                    
                    
                    ++count;
                    ++mImpl->stats.submittedByType[identifier];
                } else {
                    
                    
                    
                    
                    auto& screen = data.mContext->mScreenContext;
                    bool previous = screen.isDrawingEditorSelectionHighlight;
                    screen.isDrawingEditorSelectionHighlight = true;
                    struct Restore { bool& field; bool value; ~Restore() { field = value; } } restore{
                        screen.isDrawingEditorSelectionHighlight, previous};
                    actor->getEntityContext().getOrAddComponent<RenderPositionComponent>().mValue = pos;
                    actor->getEntityContext().getOrAddComponent<RenderRotationComponent>().mRot = actor->getRotation();
                    auto camera = data.mContext->mImpl ? data.mContext->mImpl->mCameraTargetPosition.get()
                                                     : data.mCameraTargetPos.get();
                    auto relative = pos - camera;
                    auto dispatch = [&] {
                        dispatcher->render(*data.mContext, *actor, camera, relative, actor->getRotation(), true);
                    };
                    if (actor->hasCategory(ActorCategory::Minecart)) {
                        
                        
                        if (!chunks.withEntityBlockSource(p->id, p->contentRevision,
                            [&](BlockSource& source, BlockPos const& placementOrigin) {
                                RailScope scope(source, placementOrigin);
                                dispatch();
                            })) continue;
                    } else dispatch();
                    ++count;
                    ++mImpl->stats.submittedByType[identifier];
                    ++mImpl->stats.legacyByType[identifier];
                }
            }
        }
        if (!renderer.mRequestedActors->empty()) {
            auto& history = mImpl->history;
            SwapScope h0(native.mRendererEntries.get(), history.entries);
            SwapScope h1(native.mSortedActorIDs.get(), history.ids);
            SwapScope h2(native.mSortedActorTransforms.get(), history.transforms);
            SwapScope h3(native.mSortedActorRemappingInfo.get(), history.animation);
            SwapScope h4(native.mBoneRemapping.get(), history.remapping);
            SwapScope h5(native.mBoneData.get(), history.bones);
            renderer.mOnFireActors->clear();
            renderer.mLeashedActors->clear();
            generate();
        }
        renderer.mOnFireActors.get().clear();
        renderer.mLeashedActors.get().clear();
        renderer.mRequestedActors.get().clear();
        return count;
    });
    mImpl->stats.submitted   = stats.submitted;
    mImpl->stats.highlighted = stats.highlighted;
    if (!stats.skipped.empty()) mImpl->stats.detail = stats.skipped;
}
void registerActorProjectionHooks() {
    if (installed) return;
    generateEntry = resolveProjectionEntry(
        "?generateFrame@DataDrivenRendererV2@@QEAAIAEBUGameplayRelatedRenderData@@@Z",
        "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 28 05 00 00 48 8D AC 24 ? ? ? ? "
        "44 0F 29 BD ? ? ? ? 44 0F 29 B5 ? ? ? ? 44 0F 29 AD ? ? ? ? 44 0F 29 A5 ? ? ? ? "
        "44 0F 29 9D ? ? ? ? 44 0F 29 95 ? ? ? ? 44 0F 29 8D ? ? ? ? 44 0F 29 85 ? ? ? ? "
        "0F 29 BD ? ? ? ? 0F 29 B5 ? ? ? ? 48 C7 85 ? ? ? ? ? ? ? ? 48 89 D0"
    );
    addActor  = reinterpret_cast<AddActor>(resolveProjectionEntry(
        "?tryAddActor@DataDrivenRendererV2@@QEAA_NAEAVActor@@AEBV?$shared_ptr@VActorRenderer@@@std@@W4ShadowContext@@@"
        "Z",
        "41 56 56 57 55 53 48 83 EC 40 48 8B 05 ? ? ? ? 48 31 E0 48 89 44 24 ? 4C 8B 51"
    ));
    
    
    railPositionEntry = resolveProjectionEntry(
        "?getPos@RailMovementUtility@@SA_NAEBVIConstBlockSource@@AEAVVec3@@V3@@Z",
        "41 56 56 57 53 48 81 EC 88 00 00 00 44 0F 29 44 24 ? 0F 29 7C 24 ? 0F 29 74 24 ? 4C 89 C7");
    railOffsetEntry = resolveProjectionEntry(
        "?getPosOffs@RailMovementUtility@@SA_NAEBVIConstBlockSource@@AEAVVec3@@V3@M@Z",
        "41 57 41 56 41 54 56 57 55 53 48 81 EC A0 00 00 00 44 0F 29 8C 24");
    bool const positionHooked = railPositionEntry && ProjectionRailPositionHook::hook() == 0;
    bool const offsetHooked = railOffsetEntry && ProjectionRailOffsetHook::hook() == 0;
    bool const frameHooked = generateEntry && addActor && ProjectionActorFrameHook::hook() == 0;
    installed = positionHooked && offsetHooked && frameHooked;
    if (!installed) {
        if (positionHooked) ProjectionRailPositionHook::unhook();
        if (offsetHooked) ProjectionRailOffsetHook::unhook();
        if (frameHooked) ProjectionActorFrameHook::unhook();
    }
}
void unregisterActorProjectionHooks() {
    if (installed) ProjectionActorFrameHook::unhook();
    if (installed) ProjectionRailOffsetHook::unhook();
    if (installed) ProjectionRailPositionHook::unhook();
    installed     = false;
    generateEntry = 0;
    railPositionEntry = railOffsetEntry = 0;
    addActor      = nullptr;
}
} 
