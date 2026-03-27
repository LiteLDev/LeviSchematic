#include "SchematicLoader.h"

#include "levishematic/LeviShematic.h"

#include "ll/api/service/Bedrock.h"

#include "mc/nbt/CompoundTag.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"

#include <fstream>

namespace levishematic::placement {
namespace {

auto& getLogger() {
    return levishematic::LeviShematic::getInstance().getSelf().getLogger();
}

void logFailure(
    std::string_view             operation,
    const std::filesystem::path& file,
    SchematicPlacement::Id       placementId,
    const LoadPlacementError&    error
) {
    getLogger().warn(
        "Placement operation failed [operation={}, file={}, placementId={}]: {}",
        operation,
        file.string(),
        placementId,
        error.describe(file.string())
    );
}

} // namespace

LoadPlacementDataResult SchematicLoader::loadMcstructurePlacement(
    const std::filesystem::path& path,
    BlockPos                      origin,
    const std::string&            name
) const {
    namespace fs = std::filesystem;

    auto fail = [&](LoadPlacementError error) -> LoadPlacementDataResult {
        logFailure("loader.loadMcstructure", path, 0, error);
        return LoadPlacementDataResult::failure(std::move(error));
    };

    try {
        std::error_code ec;
        if (!fs::exists(path, ec) || ec) {
            return fail({
                .code = LoadPlacementError::Code::FileNotFound,
            });
        }

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return fail({
                .code   = LoadPlacementError::Code::FileReadFailed,
                .detail = "unable to open file",
            });
        }

        auto fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        std::string rawData(fileSize, '\0');
        if (!file.read(rawData.data(), static_cast<std::streamsize>(fileSize))) {
            return fail({
                .code   = LoadPlacementError::Code::FileReadFailed,
                .detail = "read returned incomplete data",
            });
        }

        auto tagResult = CompoundTag::fromBinaryNbt(rawData, true);
        if (!tagResult) {
            return fail({
                .code = LoadPlacementError::Code::NbtParseFailed,
            });
        }

        try {
            auto level = ll::service::getLevel();
            if (!level) {
                return fail({
                    .code   = LoadPlacementError::Code::RegistryError,
                    .detail = "level service is unavailable",
                });
            }

            auto registry = level->getUnknownBlockTypeRegistry();
            StructureTemplate structureTemplate(path.filename().string(), registry);
            if (!structureTemplate.load(*tagResult)) {
                return fail({
                    .code = LoadPlacementError::Code::TemplateLoadFailed,
                });
            }

            const auto& data = structureTemplate.mStructureTemplateData;
            BlockPos     size = structureTemplate.rawSize();
            if (size.x <= 0 || size.y <= 0 || size.z <= 0) {
                return fail({
                    .code = LoadPlacementError::Code::EmptyStructure,
                });
            }

            std::vector<SchematicPlacement::LocalBlockEntry> localBlocks;
            localBlocks.reserve(static_cast<size_t>(size.x) * size.y * size.z);

            for (int y = 0; y < size.y; ++y) {
                for (int z = 0; z < size.z; ++z) {
                    for (int x = 0; x < size.x; ++x) {
                        BlockPos localPos{x, y, z};
                        auto*    block = StructureTemplate::tryGetBlockAtPos(localPos, data, registry);
                        if (!block || block->isAir()) {
                            continue;
                        }
                        localBlocks.push_back({localPos, block});
                    }
                }
            }

            std::string placementName = name.empty() ? path.stem().string() : name;
            return LoadPlacementDataResult::success(
                SchematicPlacement(std::move(localBlocks), size, origin, placementName)
            );
        } catch (const std::exception& e) {
            return fail({
                .code   = LoadPlacementError::Code::RegistryError,
                .detail = e.what(),
            });
        }
    } catch (const std::exception& e) {
        return fail({
            .code   = LoadPlacementError::Code::FileReadFailed,
            .detail = e.what(),
        });
    }
}

} // namespace levishematic::placement

