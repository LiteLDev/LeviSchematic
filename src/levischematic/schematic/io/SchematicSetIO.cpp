#include "levischematic/schematic/io/SchematicSetIO.h"
#include "levischematic/schematic/content/SchematicContent.h"
#include "levischematic/schematic/convert/LitematicConverter.h"
#include "levischematic/schematic/io/LeviSchematicFile.h"
#include "levischematic/schematic/io/StructureMerge.h"
#include "levischematic/schematic/placement/model/PlacementGeometry.h"
#include "levischematic/shared/io/AtomicFile.h"
#include "levischematic/shared/io/Utf8Path.h"
#include "mc/deps/nbt/CompoundTag.h"

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
    for (size_t i = 0; i < result->regions.size(); ++i)
        result->regions[i].content = savedSnapshot(*asset.regions[i].content, path);
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
bool saveMergesRegions(SchematicAsset const& asset, std::filesystem::path const& path) {
    return asset.regions.size() > 1 && path.extension() == ".mcstructure";
}
void saveResource(SchematicAsset const& asset, std::filesystem::path const& path, bool overwrite) {
    std::vector<std::unique_ptr<CompoundTag>> tags;
    struct Part {
        std::string id, name;
        BlockPos    offset;
    };
    std::vector<Part> parts;
    if (asset.regions.empty()) {
        tags.push_back(serializeContent(asset));
        parts.push_back({"0", asset.defaultName.empty() ? path.stem().string() : asset.defaultName, {}});
    } else
        for (auto const& r : asset.regions) {
            tags.push_back(serializeContent(*r.content));
            parts.push_back({r.id, r.name.empty() ? r.id : r.name, r.offset});
        }
    if (convert::isLitematicPath(path)) {
        std::vector<convert::StructureRegionRef> refs;
        for (size_t i = 0; i < parts.size(); ++i) refs.push_back({parts[i].name, parts[i].offset, tags[i].get()});
        convert::ConversionReport report;
        util::writeAtomic(path, convert::encodeLitematicFile(refs, path.stem().string(), report), overwrite);
    } else if (schematic::isLeviSchematicPath(path)) {
        std::vector<schematic::LeviSchematicRegionRef> refs;
        for (size_t i = 0; i < parts.size(); ++i)
            refs.push_back({parts[i].id, parts[i].name, parts[i].offset, tags[i].get()});
        schematic::LeviSchematicWriteInfo info;
        info.name = asset.defaultName.empty() ? path.stem().string() : asset.defaultName;
        util::writeAtomic(path, schematic::encodeLeviSchematicFile(refs, info), overwrite);
    } else if (path.extension() == ".mcstructure") {
        if (tags.size() == 1) util::writeAtomic(path, tags.front()->toBinaryNbt(), overwrite);
        else {
            std::vector<schematic::MergeRegion> regions;
            for (size_t i = 0; i < parts.size(); ++i) regions.push_back({parts[i].offset, tags[i].get()});
            util::writeAtomic(path, schematic::mergeStructures(regions, {})->toBinaryNbt(), overwrite);
        }
    } else throw std::runtime_error("Unsupported target format; use .levischematic, .litematic or .mcstructure.");
}
} // namespace levischematic::placement
