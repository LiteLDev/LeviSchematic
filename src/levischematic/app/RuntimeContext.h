#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace levischematic::app {

class RuntimeContext {
public:
    void setSchematicDirectory(std::filesystem::path directory);

    [[nodiscard]] bool                         hasSchematicDirectory() const;
    [[nodiscard]] std::filesystem::path const& schematicDirectory() const;
    [[nodiscard]] std::filesystem::path        resolveSchematicPath(std::string const& filename) const;
    [[nodiscard]] std::vector<std::string>     listAvailableSchematics() const;

private:
    std::filesystem::path mSchematicDirectory;
};

} // namespace levischematic::app
