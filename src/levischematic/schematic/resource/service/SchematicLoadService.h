#pragma once
#include "levischematic/shared/result/Result.h"
#include "levischematic/shared/scheduling/WorkBudget.h"
#include "mc/world/level/BlockPos.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Level;

namespace levischematic::app {
class PlacementService;
struct LoadTaskStatus {
    uint64_t    id         = 0;
    uint64_t    generation = 0;
    std::string phase;
    std::string detail;
    size_t      completed = 0;
    size_t      total     = 0;
    bool        terminal  = false;
};
enum class LoadMode { Placement, DisabledPlacement, MemoryOnly };
class SchematicLoadService {
public:
    SchematicLoadService();
    ~SchematicLoadService();
    Result<uint64_t, std::string> request(
        std::filesystem::path path,
        BlockPos              origin,
        int                   dimension,
        uint64_t              generation,
        unsigned              limit,
        LoadMode              mode = LoadMode::Placement
    );
    std::vector<std::string>
    tick(uint64_t generation, PlacementService& placements, Level& level, util::WorkBudget const& budget);
    std::optional<LoadTaskStatus> status(uint64_t id, uint64_t generation) const;
    std::vector<LoadTaskStatus>   statuses(uint64_t generation) const;
    bool                          cancel(uint64_t id, uint64_t generation);
    void                          cancelAll();

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
} // namespace levischematic::app
