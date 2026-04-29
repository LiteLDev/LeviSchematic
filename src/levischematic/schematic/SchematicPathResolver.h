#pragma once

#include <filesystem>

namespace levischematic::schematic {

class SchematicPathResolver {
public:
    explicit SchematicPathResolver(std::filesystem::path schematicDirectory = {});

    const std::filesystem::path& getSchematicDirectory() const { return mSchematicDirectory; }

    std::filesystem::path makeFilePath(const std::filesystem::path& path) const;
    std::filesystem::path resolveExistingPath(const std::filesystem::path& path) const;

private:
    std::filesystem::path withMcstructureExtension(const std::filesystem::path& path) const;

    std::filesystem::path mSchematicDirectory;
};

} // namespace levischematic::schematic
