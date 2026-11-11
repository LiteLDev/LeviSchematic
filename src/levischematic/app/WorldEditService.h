#pragma once
#include "Result.h"
#include "levischematic/util/WorkBudget.h"
#include "levischematic/worldedit/Task.h"
#include <memory>
class IClientInstance;
class Level;
namespace levischematic::editor {
struct EditorState;
}
namespace levischematic::app {
class PlacementService;
struct WorldEditOptions {
    worldedit::Operation     operation = worldedit::Operation::Paste;
    worldedit::ReplacePolicy policy    = worldedit::ReplacePolicy::TargetAir;
    worldedit::Position      offset{0, 0, 0};
    bool                     matchTypeOnly = false, layerOnly = false;
};

class WorldEditService {
    struct Impl;
    std::unique_ptr<Impl> mImpl;

public:
    WorldEditService(editor::EditorState&, PlacementService&);
    ~WorldEditService();
    WorldEditOptions&             options();
    Result<uint64_t, std::string> prepare(IClientInstance&, uint64_t generation);
    void                          start();
    void                          pause();
    void                          resume();
    void                          cancel(std::string reason = "Cancelled; world changes retained.");
    void                          tick(IClientInstance&, uint64_t generation, util::WorkBudget const&);
    void                          tickServer(Level&, uint64_t generation, util::WorkBudget const&);
    worldedit::Progress           progress() const;
    bool                          ownsInteraction() const;
    bool                          submitting() const;
    void                          sample(IClientInstance&, bool secondary);
};
} 
