#include "SchematicLoader.h"

#include "levishematic/LeviShematic.h"

#include "ll/api/service/Bedrock.h"

#include "mc/nbt/CompoundTag.h"
#include "mc/nbt/CompoundTagVariant.h"
#include "mc/nbt/ListTag.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/levelgen/structure/StructureBlockPalette.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include "mc/world/level/block/actor/BlockActor.h"

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

std::optional<verifier::ContainerSlotSnapshot> tryParseContainerSlot(CompoundTag const& tag) {
    if (!tag.contains("Slot") || !tag.contains("Count")) {
        return std::nullopt;
    }

    uint64_t nameHash = 0;
    if (tag.contains("Name")) {
        nameHash = HashedString(static_cast<std::string_view>(tag["Name"])).getHash();
    } else if (tag.contains("id")) {
        nameHash = HashedString(static_cast<std::string_view>(tag["id"])).getHash();
    }

    if (nameHash == 0) {
        return std::nullopt;
    }

    return verifier::ContainerSlotSnapshot{
        .slot         = static_cast<int>(tag["Slot"]),
        .itemNameHash = nameHash,
        .count        = static_cast<int>(tag["Count"]),
    };
}

std::optional<verifier::BlockEntitySnapshot> parseBlockEntitySnapshot(CompoundTag const& tag) {
    verifier::BlockEntitySnapshot snapshot;
    bool                          sawContainerTag = false;

    if (tag.contains("Items", Tag::List)) {
        sawContainerTag = true;
        verifier::ContainerSnapshot container;
        auto const& items = tag["Items"].get<ListTag>();
        container.slots.reserve(items.size());
        for (auto const& entry : items) {
            if (!entry || !entry.hold(Tag::Compound)) {
                continue;
            }
            auto parsedSlot = tryParseContainerSlot(entry.get<CompoundTag>());
            if (parsedSlot) {
                container.slots.push_back(*parsedSlot);
            }
        }

        snapshot.container = std::move(container);
    }

    if (!sawContainerTag) {
        return std::nullopt;
    }
    return snapshot;
}

std::optional<verifier::BlockEntitySnapshot> getBlockEntitySnapshot(
    StructureTemplateData const& data,
    int                          flatIndex
) {
    auto const* palette = data.getPalette(StructureTemplateData::DEFAULT_PALETTE_NAME());
    if (!palette) {
        return std::nullopt;
    }

    auto const* positionData = palette->getBlockPositionData(static_cast<uint64_t>(flatIndex));
    if (!positionData || !positionData->mBlockEntityData) {
        return std::nullopt;
    }

    return parseBlockEntitySnapshot(*positionData->mBlockEntityData);
}

int getFlatIndex(BlockPos const& pos, BlockPos const& size) {
    return pos.z + size.z * (pos.y + size.y * pos.x);
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
        for (int x = 0; x < size.x; ++x) {
            for (int y = 0; y < size.y; ++y) {
                for (int z = 0; z < size.z; ++z) {
                    BlockPos localPos{x, y, z};
                    auto*    block = StructureTemplate::tryGetBlockAtPos(localPos, data, registry);
                    if (!block || block->isAir()) {
                        continue;
                    }
                    asset->localBlocks.push_back({
                        .localPos    = localPos,
                        .renderBlock = block,
                        .compareSpec = verifier::buildCompareSpecFromBlock(*block),
                        .blockEntity = getBlockEntitySnapshot(data, getFlatIndex(localPos, size)),
                    });
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
