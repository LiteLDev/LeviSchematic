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
    std::filesystem::path const& file,
    LoadPlacementError const&    error
) {
    getLogger().warn(
        "Placement operation failed [operation={}, file={}]: {}",
        operation,
        file.string(),
        error.describe(file.string())
    );
}

} // namespace

LoadAssetResult SchematicLoader::loadMcstructureAsset(std::filesystem::path const& path) const {
    namespace fs = std::filesystem;

    auto fail = [&](LoadPlacementError error) -> LoadAssetResult {
        logFailure("loader.loadMcstructureAsset", path, error);
        return LoadAssetResult::failure(std::move(error));
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

        auto size = structureTemplate.rawSize();
        if (size.x <= 0 || size.y <= 0 || size.z <= 0) {
            return fail({
                .code = LoadPlacementError::Code::EmptyStructure,
            });
        }

        auto asset         = std::make_shared<SchematicAsset>();
        asset->size        = size;
        asset->defaultName = path.stem().string();
        asset->localBlocks.reserve(static_cast<size_t>(size.x) * size.y * size.z);

        auto const& data = structureTemplate.mStructureTemplateData;
        for (int y = 0; y < size.y; ++y) {
            for (int z = 0; z < size.z; ++z) {
                for (int x = 0; x < size.x; ++x) {
                    BlockPos localPos{x, y, z};
                    auto*    block = StructureTemplate::tryGetBlockAtPos(localPos, data, registry);
                    if (!block || block->isAir()) {
                        continue;
                    }
                    asset->localBlocks.push_back({localPos, block});
                }
            }
        }

        return LoadAssetResult::success(std::move(asset));
    } catch (std::exception const& e) {
        return fail({
            .code   = LoadPlacementError::Code::FileReadFailed,
            .detail = e.what(),
        });
    }
}

} // namespace levishematic::placement
