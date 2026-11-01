#pragma once
#include "levischematic/app/Result.h"
#include "levischematic/util/WorkBudget.h"
#include "mc/world/level/BlockPos.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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

class SchematicLoadService {
public:
    SchematicLoadService();

    ~SchematicLoadService();

    Result<uint64_t, std::string>
    request(std::filesystem::path path, BlockPos origin, int dimension, uint64_t generation, unsigned limit);

    std::vector<std::string> tick(uint64_t generation, PlacementService& placements, util::WorkBudget const& budget);

    std::optional<LoadTaskStatus> status(uint64_t id, uint64_t generation) const;

    bool cancel(uint64_t id, uint64_t generation);

    void cancelAll();

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
} 
