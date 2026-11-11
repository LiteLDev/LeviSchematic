#include "SchematicLoader.h"
#include "ll/api/service/Bedrock.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/levelgen/structure/StructureBlockPalette.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include <fstream>
#include <limits>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "levischematic/util/Utf8Path.h"
#include "PlacementGeometry.h"
#include "SchematicSetIO.h"

namespace levischematic::placement {
Expected<AssetPreparation, LoadPlacementError>
SchematicLoader::read(std::filesystem::path const& path, std::stop_token stop) {
    using Result = Expected<AssetPreparation, LoadPlacementError>;
    try {
        if (path.filename().string().ends_with(".schemset.json")) {
            std::ifstream file(path);
            auto document = nlohmann::json::parse(file);
            if (document.at("version") != 1) throw std::runtime_error("Unsupported schematic set version.");
            AssetPreparation result;
            result.asset = std::make_shared<SchematicAsset>();
            result.asset->defaultName = document.value("name", path.stem().string());
            auto origin = document.value("origin",std::array<int,3>{});
            std::unordered_set<std::string> ids;
            auto root = std::filesystem::weakly_canonical(path.parent_path());
            for (auto const& entry : document.at("regions")) {
                if (stop.stop_requested()) throw std::runtime_error("Cancelled.");
                auto id=entry.at("id").get<std::string>();
                if (id.empty() || !ids.insert(id).second) throw std::runtime_error("Duplicate/empty region ID.");
                auto relative=util::pathFromUtf8(entry.at("file").get<std::string>());
                auto target=std::filesystem::weakly_canonical(root/relative);
                auto within=target.lexically_relative(root);
                if (relative.is_absolute() || within.empty() || *within.begin()==".." || target.extension()!=".mcstructure")
                    throw std::runtime_error("Region must reference an mcstructure inside the set directory.");
                auto child=read(target,stop);
                if (!child) return Result::failure(child.error());
                auto offset=entry.at("offset").get<std::array<int,3>>();
                auto pos=checkedPosition({int64_t(offset[0])-origin[0],int64_t(offset[1])-origin[1],int64_t(offset[2])-origin[2]});
                if (!pos) throw std::runtime_error("Region offset overflow.");
                result.asset->regions.push_back({id,entry.value("name",id),*pos,child.value().asset});
                auto size=child.value().asset->size;
                auto end=checkedPosition({int64_t(pos->x)+size.x,int64_t(pos->y)+size.y,int64_t(pos->z)+size.z});
                if (!end) throw std::runtime_error("Region extent overflow.");
                auto& extent=result.asset->size;
                extent={std::max(extent.x,end->x),std::max(extent.y,end->y),std::max(extent.z,end->z)};
                result.regions.push_back(std::move(child).value());
            }
            if (result.regions.empty()) throw std::runtime_error("Schematic set has no regions.");
            result.asset->size.x=std::max(1,result.asset->size.x);
            result.asset->size.y=std::max(1,result.asset->size.y);
            result.asset->size.z=std::max(1,result.asset->size.z);
            result.asset->sourcePath=path;
            result.asset->sourceStamp=fileStamp(path);
            return Result::success(std::move(result));
        }
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) return Result::failure({LoadPlacementError::Code::FileNotFound});
        auto length = input.tellg();
        if (length <= 0 || static_cast<uint64_t>(length) > std::string{}.max_size())
            return Result::failure({LoadPlacementError::Code::FileReadFailed, "invalid file length"});
        input.seekg(0);
        std::string bytes;
        char        buffer[65536];
        while (input && !stop.stop_requested()) {
            input.read(buffer, sizeof(buffer));
            bytes.append(buffer, static_cast<size_t>(input.gcount()));
        }
        if (stop.stop_requested()) return Result::failure({LoadPlacementError::Code::FileReadFailed, "cancelled"});
        if (input.bad() || bytes.size() != static_cast<uint64_t>(length))
            return Result::failure({LoadPlacementError::Code::FileReadFailed, "incomplete read"});
        auto tag = CompoundTag::fromBinaryNbt(bytes, true);
        if (!tag) return Result::failure({LoadPlacementError::Code::NbtParseFailed});
        auto result=readTag(*tag,path,stop);
        if (result) { result.value().asset->sourcePath=path;result.value().asset->sourceStamp=fileStamp(path); }
        return result;
    } catch (std::exception const& e) {
        return Result::failure({LoadPlacementError::Code::FileReadFailed, e.what()});
    }
}
Expected<AssetPreparation, LoadPlacementError>
SchematicLoader::readTag(CompoundTag const& tag, std::filesystem::path const& path, std::stop_token stop) {
    using Result = Expected<AssetPreparation, LoadPlacementError>;
    try {
        
        auto structure = std::make_shared<StructureTemplate>(
            path.stem().string(),
            Bedrock::NonOwnerPointer<IUnknownBlockTypeRegistry>{}
        );
        if (!structure->load(tag)) return Result::failure({LoadPlacementError::Code::TemplateLoadFailed});
        auto size = structure->rawSize();
        if (size.x <= 0 || size.y <= 0 || size.z <= 0)
            return Result::failure({LoadPlacementError::Code::EmptyStructure});
        uint64_t volume = static_cast<uint64_t>(size.x) * static_cast<uint64_t>(size.y);
        if (volume > static_cast<uint64_t>(std::numeric_limits<int>::max()) / static_cast<uint64_t>(size.z))
            return Result::failure(
                {LoadPlacementError::Code::TemplateLoadFailed, "structure volume exceeds supported index range"}
            );
        volume *= size.z;
        if (structure->mStructureTemplateData->mBlockIndices->size() != volume)
            return Result::failure(
                {LoadPlacementError::Code::TemplateLoadFailed, "block index count does not match size"}
            );
        auto const* palette =
            structure->mStructureTemplateData->getPalette(StructureTemplateData::DEFAULT_PALETTE_NAME());
        if (!palette)
            return Result::failure({LoadPlacementError::Code::TemplateLoadFailed, "default palette is missing"});
        auto count = palette->mStructurePaletteIdToSerializationId->size();
        for (int index : structure->mStructureTemplateData->mBlockIndices.get()) {
            if (stop.stop_requested()) return Result::failure({LoadPlacementError::Code::FileReadFailed, "cancelled"});
            if (index < -1 || (index >= 0 && static_cast<size_t>(index) >= count))
                return Result::failure({LoadPlacementError::Code::TemplateLoadFailed, "invalid palette index"});
        }
        AssetPreparation result;
        result.asset              = std::make_shared<SchematicAsset>();
        result.asset->size        = size;
        result.asset->defaultName = path.stem().string();
        result.asset->structure   = std::move(structure);
        result.palette.resize(count);
        return Result::success(std::move(result));
    } catch (std::exception const& e) {
        return Result::failure({LoadPlacementError::Code::FileReadFailed, e.what()});
    }
}

bool SchematicLoader::advance(AssetPreparation& pending, util::WorkBudget const& budget) {
    if (!pending.regions.empty()) {
        while (pending.regionCursor<pending.regions.size() && budget.available()) {
            if (!advance(pending.regions[pending.regionCursor],budget)) return false;
            ++pending.regionCursor;
        }
        return pending.regionCursor==pending.regions.size();
    }
    auto level = ll::service::getLevel();
    if (!level) throw std::runtime_error("World registry is unavailable.");
    auto        registry = level->getUnknownBlockTypeRegistry();
    auto const& data     = pending.asset->structure->mStructureTemplateData.get();
    auto const* palette  = data.getPalette(StructureTemplateData::DEFAULT_PALETTE_NAME());
    while (pending.paletteCursor < pending.palette.size() && budget.available()) {
        auto i             = pending.paletteCursor++;
        pending.palette[i] = palette->tryGetBlock(i, registry);
    }
    if (pending.paletteCursor < pending.palette.size()) return false;
    if (pending.asset->palette.empty()) pending.asset->palette = pending.palette;
    auto const& indices = data.mBlockIndices.get();
    auto        size    = pending.asset->size;
    while (pending.blockCursor < indices.size() && budget.available()) {
        auto  flat  = pending.blockCursor++;
        auto  index = indices[flat];
        pending.asset->indices.push_back(index);
        auto* block = index >= 0 ? pending.palette[static_cast<size_t>(index)] : nullptr;
        if (!block || block->isAir()) continue;
        BlockPos pos{
            static_cast<int>(flat / (static_cast<size_t>(size.y) * size.z)),
            static_cast<int>((flat / size.z) % size.y),
            static_cast<int>(flat % size.z)
        };
        pending.asset->localBlocks.push_back({pos, block});
    }
    return pending.blockCursor == indices.size();
}
} 
