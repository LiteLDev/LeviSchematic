#pragma once
#include "levischematic/shared/result/Result.h"
#include "levischematic/shared/scheduling/WorkBudget.h"
#include "mc/world/level/BlockPos.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
class ClientInstance;
namespace levischematic::editor {
struct EditorState;
}
namespace levischematic::app {
class SelectionService;
class SelectionSaveService;
class SchematicLoadService;
class PlacementService;
class WorldEditService;
struct ProjectServices {
    SelectionService&     selection;
    SelectionSaveService& saves;
    SchematicLoadService& loads;
    PlacementService&     placement;
    WorldEditService&     worldEdit;
    uint64_t              generation;
};
struct ProjectVersionView {
    int         id = -1;
    std::string name, description;
};
struct ProjectView {
    bool                            open = false, saving = false;
    std::string                     name, detail, work;
    BlockPos                        origin{0, 0, 0};
    int                             current = -1, lastPasted = -1;
    std::vector<ProjectVersionView> versions;
};
class ProjectService {
    struct Impl;
    std::unique_ptr<Impl> mImpl;

public:
    explicit ProjectService(std::filesystem::path configDirectory);
    ~ProjectService();
    void                      tick(ProjectServices const&, ClientInstance&, util::WorkBudget const&);
    ProjectView               view() const;
    std::vector<std::string>  list() const;
    StatusResult<std::string> create(std::string name, BlockPos origin);
    StatusResult<std::string> open(std::string name);
    StatusResult<std::string> rename(std::string name);
    StatusResult<std::string> setOrigin(BlockPos);
    StatusResult<std::string> setWork(std::string);
    StatusResult<std::string> saveVersion(ProjectServices const&, std::string name, std::string description);
    StatusResult<std::string> checkout(ProjectServices const&, int version);
    StatusResult<std::string> restoreSelection(ProjectServices const&);
    StatusResult<std::string> paste(ProjectServices const&, ClientInstance&);
    void                      cancel(ProjectServices const&);
};
} // namespace levischematic::app
