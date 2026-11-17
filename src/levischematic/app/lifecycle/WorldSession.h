#pragma once
#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/app/lifecycle/AppKernel.h"
#include "levischematic/render/actor/ActorProjection.h"
#include "levischematic/render/chunk/SchematicChunkRenderer.h"
#include "levischematic/schematic/io/SchematicLoader.h"
#include "levischematic/schematic/placement/store/PlacementStore.h"
#include "levischematic/schematic/placement/view/ProjectionRefresh.h"
#include "levischematic/selection/capture/SelectionExporter.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/service/ServerInfo.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/player/LocalPlayer.h"
#include <stdexcept>

namespace levischematic::app {
class AppKernel::WorldSession {
public:
    RuntimeContext                                  runtime;
    std::unique_ptr<editor::EditorState>            state;
    std::unique_ptr<placement::PlacementStore>      placementStore;
    std::unique_ptr<selection::SelectionExporter>   selectionExporter;
    std::unique_ptr<render::SchematicChunkRenderer> renderer;
    std::unique_ptr<PlacementService>               placementService;
    std::unique_ptr<SelectionService>               selectionService;
    std::unique_ptr<ViewService>                    viewService;
    std::unique_ptr<verifier::VerifierService>      verifierService;
    std::unique_ptr<SessionPersistence>             persistence;
    input::InputHandler                             input;
    std::unique_ptr<ToolService>                    tools;
    std::unique_ptr<SurvivalService>                survival;
    std::unique_ptr<WorldEditService>               worldEdit;
    ProjectService                                  projects{LeviSchematic::getInstance().getSelf().getConfigDir()};
    SignService                                     signs;
    uint64_t                                        worldNotifiedId    = 0;
    worldedit::Phase                                worldNotifiedPhase = worldedit::Phase::Idle;
    render::ActorProjection                         actors;
    bool                                            loadFirst              = false;
    bool                                            commandsRegistered     = false;
    bool                                            directoryWarningLogged = false;
    void                                            configureSchematicDirectory();
    WorldSession();
    ~WorldSession() = default;
};

}
