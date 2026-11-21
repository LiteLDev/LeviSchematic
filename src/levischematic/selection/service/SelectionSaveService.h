#pragma once
#include "levischematic/selection/model/SelectionState.h"
#include "levischematic/shared/result/Result.h"
#include "levischematic/shared/scheduling/WorkBudget.h"
#include <filesystem>
#include <memory>
#include <string>
class ClientInstance;
class Level;
class Dimension;
namespace levischematic::app {
struct SaveOptions {
    bool includeEntities = true, overwrite = false;
};
struct SaveProgress {
    uint64_t              id = 0;
    std::string           phase, detail;
    size_t                completed = 0, total = 0;
    bool                  terminal = true;
    std::filesystem::path path;
};
class SelectionSaveService {
public:
    SelectionSaveService();
    ~SelectionSaveService();
    Result<uint64_t, std::string> request(
        selection::SelectionState const&,
        std::filesystem::path directory,
        std::string           name,
        int                   dimension,
        uint64_t              generation,
        SaveOptions = {}
    );
    void         tick(ClientInstance&, uint64_t generation, util::WorkBudget const&);
    void         tickServer(Level&, uint64_t generation, util::WorkBudget const&);
    void         cancel();
    SaveProgress progress() const;

private:
    void advanceGroup(ClientInstance&, uint64_t generation, util::WorkBudget const&);
    void startLitematicMerge();
    void advance(Dimension&, Level&, util::WorkBudget const&);
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
}
