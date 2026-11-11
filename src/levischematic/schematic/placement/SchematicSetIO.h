#pragma once
#include "SchematicLoader.h"
#include <nlohmann/json.hpp>

namespace levischematic::placement {
std::string                                    fileStamp(std::filesystem::path const&);
nlohmann::json                                 encodeDraft(SchematicAsset const&);
Expected<AssetPreparation, LoadPlacementError> decodeDraft(nlohmann::json const&, std::filesystem::path const&);

void                                  saveResource(SchematicAsset const&, std::filesystem::path const&, bool overwrite);
std::shared_ptr<const SchematicAsset> savedSnapshot(SchematicAsset const&, std::filesystem::path const&);
bool                                  resourceSourceChanged(SchematicAsset const&);
} 
