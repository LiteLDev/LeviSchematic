#pragma once
#include "Result.h"
#include "levischematic/editor/LayerRange.h"
#include "levischematic/schematic/placement/PlacementStore.h"
#include "levischematic/schematic/placement/ResourceState.h"
#include "levischematic/schematic/placement/SchematicContent.h"
#include <map>

namespace levischematic::app {
using ResourceRecord = placement::ResourceRecord;
struct ContentProgress {
    bool        running = false;
    size_t      scanned = 0, total = 0, changed = 0;
    std::string detail;
};


class ResourceService {
    placement::PlacementStore&          mStore;
    std::map<uint64_t, ResourceRecord>& mResources;
    uint64_t&                           mNext;
    struct Edit;
    struct Reload;
    std::unique_ptr<Edit>   mEdit;
    std::unique_ptr<Reload> mReload;
    ContentProgress         mProgress;
    void                    publish(ResourceRecord&, std::shared_ptr<const placement::SchematicAsset>);

public:
    ResourceService(placement::PlacementStore& store, placement::ResourceState&);
    ~ResourceService();
    std::shared_ptr<const placement::SchematicAsset>
                                  adopt(std::shared_ptr<const placement::SchematicAsset>, std::filesystem::path const&);
    std::vector<ResourceRecord>   list();
    Result<uint64_t, std::string> createPlacement(uint64_t resource, BlockPos origin, int dimension);
    StatusResult<std::string>     unload(uint64_t resource, bool discard, bool removePlacements);
    StatusResult<std::string>     save(uint64_t resource, std::filesystem::path path, bool overwrite);
    StatusResult<std::string>     saveAs(placement::PlacementId, std::filesystem::path, bool overwrite);
    StatusResult<std::string>     reload(uint64_t resource, bool discard);
    StatusResult<std::string>     edit(placement::EditRequest const&, editor::LayerRange const&);
    void                          tick(util::WorkBudget const&);
    void                          cancel();
    ContentProgress               progress() const { return mProgress; }
};
} 
