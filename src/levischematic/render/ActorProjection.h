#pragma once
#include "levischematic/util/WorkBudget.h"
#include <functional>
#include <memory>
#include <map>
#include <string>

class ClientInstance;
class DataDrivenRendererV2;
struct GameplayRelatedRenderData;
namespace levischematic::app {
class PlacementService;
}
namespace levischematic::render {
class SchematicChunkRenderer;
struct ActorProjectionStats {
    size_t      loaded = 0, skipped = 0, submitted = 0, highlighted = 0;
    std::string detail;
    std::map<std::string, size_t> submittedByType, missingRendererByType, legacyByType;
};



class ActorProjection {
    struct Impl;
    std::unique_ptr<Impl> mImpl;

public:
    ActorProjection();
    ~ActorProjection();
    void tick(ClientInstance&, app::PlacementService const&, util::WorkBudget const&);
    void clear();
    void draw(
        DataDrivenRendererV2&,
        GameplayRelatedRenderData const&,
        app::PlacementService const&,
        SchematicChunkRenderer&,
        std::function<void()> const& generate
    );
    ActorProjectionStats stats() const;
};
void registerActorProjectionHooks();
void unregisterActorProjectionHooks();
} 
