#pragma once
#include "levischematic/schematic/placement/model/PlacementModel.h"
#include <map>
namespace levischematic::placement {
struct ResourceRecord {
    uint64_t                              id = 0;
    std::filesystem::path                 path;
    std::shared_ptr<const SchematicAsset> content;
};
struct ResourceState {
    std::map<uint64_t, ResourceRecord> records;
    uint64_t                           nextId = 1;
};
} // namespace levischematic::placement
