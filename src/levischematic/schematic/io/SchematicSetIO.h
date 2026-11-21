#pragma once
#include "levischematic/schematic/io/SchematicLoader.h"
#include <nlohmann/json.hpp>
#include <span>

namespace levischematic::placement {
struct SchematicSetEntry {
    std::string           id;
    std::string           name;
    BlockPos              offset;
    std::filesystem::path file;
};
struct SchematicSetManifest {
    std::string                    name;
    std::vector<SchematicSetEntry> regions;
};
SchematicSetManifest readSchematicSetManifest(std::filesystem::path const& path);
struct SchematicSetRegionData {
    std::string        id;
    std::string        name;
    BlockPos           offset;
    CompoundTag const* tag = nullptr;
};
void writeSchematicSet(
    std::filesystem::path const&            path,
    std::string const&                      name,
    std::span<SchematicSetRegionData const> regions,
    bool                                    overwrite
);

std::string                                    fileStamp(std::filesystem::path const&);
nlohmann::json                                 encodeDraft(SchematicAsset const&);
Expected<AssetPreparation, LoadPlacementError> decodeDraft(nlohmann::json const&, std::filesystem::path const&);
void                                  saveResource(SchematicAsset const&, std::filesystem::path const&, bool overwrite);
std::shared_ptr<const SchematicAsset> savedSnapshot(SchematicAsset const&, std::filesystem::path const&);
bool                                  resourceSourceChanged(SchematicAsset const&);
}
