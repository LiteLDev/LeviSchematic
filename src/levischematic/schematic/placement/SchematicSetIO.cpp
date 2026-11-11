#include "SchematicSetIO.h"
#include "SchematicContent.h"
#include "levischematic/util/AtomicFile.h"
#include "levischematic/util/Utf8Path.h"
#include "mc/deps/nbt/CompoundTag.h"
#include <chrono>
#include <fstream>

namespace levischematic::placement {
bool resourceSourceChanged(SchematicAsset const& asset) {
    if (asset.sourceConflict || (!asset.sourcePath.empty() && fileStamp(asset.sourcePath) != asset.sourceStamp))
        return true;
    for (auto const& r : asset.regions)
        if (resourceSourceChanged(*r.content)) return true;
    return false;
}
std::shared_ptr<const SchematicAsset> savedSnapshot(SchematicAsset const& asset, std::filesystem::path const& path) {
    auto result            = std::make_shared<SchematicAsset>(asset);
    result->modified       = false;
    result->sourceConflict = false;
    result->sourcePath     = path;
    result->sourceStamp    = fileStamp(path);
    if (!asset.regions.empty()) {
        std::ifstream file(path);
        auto          manifest = nlohmann::json::parse(file);
        for (size_t i = 0; i < result->regions.size(); ++i) {
            auto relative              = util::pathFromUtf8(manifest.at("regions").at(i).at("file").get<std::string>());
            result->regions[i].content = savedSnapshot(*asset.regions[i].content, path.parent_path() / relative);
        }
    }
    return result;
}
std::string fileStamp(std::filesystem::path const& path) {
    std::error_code error;
    auto            size = std::filesystem::file_size(path, error);
    if (error) return "missing";
    auto time = std::filesystem::last_write_time(path, error);
    return error ? "unavailable" : std::to_string(size) + ":" + std::to_string(time.time_since_epoch().count());
}
nlohmann::json encodeDraft(SchematicAsset const& asset) {
    auto           source = asset.sourcePath.generic_u8string();
    nlohmann::json result{
        {"version", 1},
        {"stamp", asset.sourceStamp},
        {"modified", asset.modified},
        {"source", std::string(reinterpret_cast<char const*>(source.data()), source.size())},
        {"name", asset.defaultName},
        {"size", {asset.size.x, asset.size.y, asset.size.z}}
    };
    if (asset.regions.empty()) result["nbt"] = serializeContent(asset)->toSnbt();
    else {
        result["regions"] = nlohmann::json::array();
        for (auto const& r : asset.regions)
            result["regions"].push_back({
                {"id",      r.id                                },
                {"name",    r.name                              },
                {"offset",  {r.offset.x, r.offset.y, r.offset.z}},
                {"content", encodeDraft(*r.content)             }
            });
    }
    return result;
}
Expected<AssetPreparation, LoadPlacementError>
decodeDraft(nlohmann::json const& document, std::filesystem::path const& path) {
    using Result = Expected<AssetPreparation, LoadPlacementError>;
    try {
        if (document.at("version") != 1) throw std::runtime_error("Invalid resource draft version.");
        AssetPreparation result;
        if (document.contains("nbt")) {
            auto tag = CompoundTag::fromSnbt(document.at("nbt").get<std::string>());
            if (!tag) throw std::runtime_error("Invalid draft NBT.");
            auto parsed = SchematicLoader::readTag(*tag, path, {});
            if (!parsed) return parsed;
            result = std::move(parsed).value();
        } else {
            result.asset = std::make_shared<SchematicAsset>();
            auto size    = document.at("size").get<std::array<int, 3>>();
            if (size[0] <= 0 || size[1] <= 0 || size[2] <= 0) throw std::runtime_error("Invalid draft dimensions.");
            result.asset->size = {size[0], size[1], size[2]};
            for (auto const& r : document.at("regions")) {
                auto childPath = util::pathFromUtf8(r.at("content").value("source", std::string{}));
                auto pending   = decodeDraft(r.at("content"), childPath);
                if (!pending) return pending;
                auto offset = r.at("offset").get<std::array<int, 3>>();
                result.asset->regions.push_back({
                    r.at("id"),
                    r.at("name"),
                    {offset[0], offset[1], offset[2]},
                    pending.value().asset
                });
                result.regions.push_back(std::move(pending).value());
            }
        }
        result.asset->defaultName    = document.at("name");
        result.asset->modified       = document.at("modified");
        result.asset->sourceStamp    = document.at("stamp");
        result.asset->sourcePath     = path;
        result.asset->sourceConflict = result.asset->sourceStamp != fileStamp(path);
        return Result::success(std::move(result));
    } catch (std::exception const& e) {
        return Result::failure({LoadPlacementError::Code::NbtParseFailed, e.what()});
    }
}
void saveResource(SchematicAsset const& asset, std::filesystem::path const& path, bool overwrite) {
    if (asset.regions.empty()) {
        if (path.extension() != ".mcstructure") throw std::runtime_error("A single resource requires .mcstructure.");
        util::writeAtomic(path, serializeContent(asset)->toBinaryNbt(), overwrite);
        return;
    }
    if (!path.filename().string().ends_with(".schemset.json"))
        throw std::runtime_error("Multiple regions require .schemset.json.");
    auto generation = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    auto directory  = path.parent_path() / (path.stem().string() + ".regions-" + generation);
    
    
    if (!std::filesystem::create_directories(directory))
        throw std::runtime_error("Generation directory already exists.");
    nlohmann::json manifest{
        {"version", 1                      },
        {"name",    asset.defaultName      },
        {"origin",  {0, 0, 0}              },
        {"regions", nlohmann::json::array()}
    };
    size_t index = 0;
    for (auto const& r : asset.regions) {
        auto file = directory / (std::to_string(index++) + ".mcstructure");
        util::writeAtomic(file, serializeContent(*r.content)->toBinaryNbt(), false);
        auto relative = file.lexically_relative(path.parent_path()).generic_u8string();
        manifest["regions"].push_back({
            {"id", r.id},
            {"name", r.name},
            {"offset", {r.offset.x, r.offset.y, r.offset.z}},
            {"file", std::string(reinterpret_cast<char const*>(relative.data()), relative.size())}
        });
    }
    util::writeAtomic(path, manifest.dump(2), overwrite);
}
} 
