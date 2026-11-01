#pragma once

#include <filesystem>

namespace levischematic::schematic {


class SchematicPathResolver {
public:
    explicit SchematicPathResolver(std::filesystem::path schematicDirectory = {});


    std::filesystem::path const& getSchematicDirectory() const { return mSchematicDirectory; }


    std::filesystem::path makeFilePath(std::filesystem::path const& path) const;


    std::filesystem::path resolveExistingPath(std::filesystem::path const& path) const;

private:
    std::filesystem::path withMcstructureExtension(std::filesystem::path const& path) const;

    std::filesystem::path mSchematicDirectory;
};

} 
