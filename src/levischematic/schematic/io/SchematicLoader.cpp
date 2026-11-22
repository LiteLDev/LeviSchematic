#include "levischematic/schematic/io/SchematicLoader.h"
#include "levischematic/schematic/convert/LitematicConverter.h"
#include "levischematic/schematic/io/LeviSchematicFile.h"
#include "levischematic/schematic/io/SchematicSetIO.h"
#include "levischematic/schematic/placement/model/PlacementGeometry.h"
#include "levischematic/shared/io/Utf8Path.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/levelgen/structure/StructureBlockPalette.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace levischematic::placement {
Expected<AssetPreparation, LoadPlacementError>
SchematicLoader::read(std::filesystem::path const& path, std::stop_token stop) {
    using Result = Expected<AssetPreparation, LoadPlacementError>;
    try {
        auto assemble = [&](std::string name, auto&& regions) -> Result {
            AssetPreparation result;
            result.asset              = std::make_shared<SchematicAsset>();
            result.asset->defaultName = std::move(name);
            for (auto& [id, regionName, pos, child] : regions) {
                if (stop.stop_requested()) throw std::runtime_error("Cancelled.");
                if (!child) return Result::failure(child.error());
                result.asset->regions.push_back({id, regionName, pos, child.value().asset});
                auto size = child.value().asset->size;
                auto end = checkedPosition({int64_t(pos.x) + size.x, int64_t(pos.y) + size.y, int64_t(pos.z) + size.z});
                if (!end) throw std::runtime_error("Region extent overflow.");
                auto& extent = result.asset->size;
                extent       = {std::max(extent.x, end->x), std::max(extent.y, end->y), std::max(extent.z, end->z)};
                result.regions.push_back(std::move(child).value());
            }
            if (result.regions.empty()) throw std::runtime_error("Schematic set has no regions.");
            result.asset->size.x      = std::max(1, result.asset->size.x);
            result.asset->size.y      = std::max(1, result.asset->size.y);
            result.asset->size.z      = std::max(1, result.asset->size.z);
            result.asset->sourcePath  = path;
            result.asset->sourceStamp = fileStamp(path);
            return Result::success(std::move(result));
        };
        struct Pending {
            std::string                                    id;
            std::string                                    name;
            BlockPos                                       pos;
            Expected<AssetPreparation, LoadPlacementError> child;
        };
        if (schematic::isLeviSchematicPath(path)) {
            auto document = schematic::readLeviSchematic(path, stop);
            auto stamp    = fileStamp(path);
            auto parse    = [&](CompoundTag const& tag) {
                auto child = readTag(tag, path, stop);
                if (child) {
                    child.value().asset->sourcePath  = path;
                    child.value().asset->sourceStamp = stamp;
                }
                return child;
            };
            if (document.regions.size() == 1) {
                auto result = parse(*document.regions.front().tag);
                if (result) result.value().asset->defaultName = document.name;
                return result;
            }
            std::vector<Pending> regions;
            for (auto& r : document.regions) {
                regions.push_back({r.id, r.name, r.offset, parse(*r.tag)});
                if (!regions.back().child) return Result::failure(regions.back().child.error());
            }
            return assemble(document.name, regions);
        }
        if (convert::isLitematicPath(path)) {
            convert::ConversionReport report;
            auto                      structures = convert::readLitematic(path, stop, report);
            auto                      stamp      = fileStamp(path);
            auto                      parse      = [&](CompoundTag const& tag) {
                auto child = readTag(tag, path, stop);
                if (child) {
                    child.value().asset->sourcePath  = path;
                    child.value().asset->sourceStamp = stamp;
                }
                return child;
            };
            Result result = Result::failure({LoadPlacementError::Code::EmptyStructure});
            if (structures.size() == 1) result = parse(*structures.front().tag);
            else {
                std::vector<Pending> regions;
                for (size_t i = 0; i < structures.size(); ++i) {
                    auto& s = structures[i];
                    regions.push_back({std::to_string(i), s.name, s.offset, parse(*s.tag)});
                    if (!regions.back().child) return Result::failure(regions.back().child.error());
                }
                result = assemble(path.stem().string(), regions);
            }
            if (result) result.value().notice = report.summary();
            return result;
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
        auto result = readTag(*tag, path, stop);
        if (result) {
            result.value().asset->sourcePath  = path;
            result.value().asset->sourceStamp = fileStamp(path);
        }
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

bool SchematicLoader::advance(AssetPreparation& pending, Level& level, util::WorkBudget const& budget) {
    if (!pending.regions.empty()) {
        while (pending.regionCursor < pending.regions.size() && budget.available()) {
            if (!advance(pending.regions[pending.regionCursor], level, budget)) return false;
            ++pending.regionCursor;
        }
        return pending.regionCursor == pending.regions.size();
    }
    auto        registry = level.getUnknownBlockTypeRegistry();
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
        auto flat  = pending.blockCursor++;
        auto index = indices[flat];
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
} // namespace levischematic::placement
