#include "SchematicLoader.h"
#include "ll/api/service/Bedrock.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/levelgen/structure/StructureBlockPalette.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include <fstream>
#include <limits>
#include <stdexcept>

namespace levischematic::placement {
Expected<AssetPreparation, LoadPlacementError>
SchematicLoader::read(std::filesystem::path const& path, std::stop_token stop) {
    using Result = Expected<AssetPreparation, LoadPlacementError>;
    try {
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
        
        auto structure = std::make_shared<StructureTemplate>(
            path.stem().string(),
            Bedrock::NonOwnerPointer<IUnknownBlockTypeRegistry>{}
        );
        if (!structure->load(*tag)) return Result::failure({LoadPlacementError::Code::TemplateLoadFailed});
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
    auto const& indices = data.mBlockIndices.get();
    auto        size    = pending.asset->size;
    while (pending.blockCursor < indices.size() && budget.available()) {
        auto  flat  = pending.blockCursor++;
        auto  index = indices[flat];
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
