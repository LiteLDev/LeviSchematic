#pragma once
#include "levischematic/schematic/convert/BlockMapping.h"
#include "mc/world/level/BlockPos.h"
#include <filesystem>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

class CompoundTag;

namespace levischematic::convert {
struct ConversionReport {
    size_t                   fallbackStates       = 0;
    size_t                   unmappedStates       = 0;
    size_t                   droppedBlockEntities = 0;
    size_t                   droppedEntities      = 0;
    size_t                   droppedTicks         = 0;
    std::vector<std::string> unmappedSamples;

    [[nodiscard]] std::string summary() const;
};

struct StructureRegion {
    std::string                  name;
    BlockPos                     offset;
    std::unique_ptr<CompoundTag> tag;
};
struct StructureRegionRef {
    std::string        name;
    BlockPos           offset;
    CompoundTag const* tag = nullptr;
};

void                                setBlockMappingPath(std::filesystem::path path);
std::shared_ptr<BlockMapping const> blockMapping();
void                                captureBedrockBlockVersion();

[[nodiscard]] bool           isLitematicPath(std::filesystem::path const& path);
std::unique_ptr<CompoundTag> readMcstructureFile(std::filesystem::path const& path);

std::vector<StructureRegion>
readLitematic(std::filesystem::path const& path, std::stop_token stop, ConversionReport& report);
std::string
encodeLitematicFile(std::span<StructureRegionRef const> regions, std::string const& name, ConversionReport& report);

void convertFile(
    std::filesystem::path const& source,
    std::filesystem::path const& target,
    bool                         overwrite,
    std::stop_token              stop,
    ConversionReport&            report
);
}
