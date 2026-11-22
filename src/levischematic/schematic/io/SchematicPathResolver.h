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
    static std::filesystem::path withExtension(const std::filesystem::path& path, char const* extension);

    std::filesystem::path mSchematicDirectory;
};

}
