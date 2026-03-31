#include "RuntimeContext.h"

#include "levishematic/schematic/SchematicPathResolver.h"

#include <algorithm>

namespace levishematic::app {

void RuntimeContext::setSchematicDirectory(std::filesystem::path directory) {
    mSchematicDirectory = std::move(directory);
    if (!mSchematicDirectory.empty()) {
        mSchematicDirectory = mSchematicDirectory.lexically_normal();
    }
}

bool RuntimeContext::hasSchematicDirectory() const {
    return !mSchematicDirectory.empty();
}

std::filesystem::path const& RuntimeContext::schematicDirectory() const {
    return mSchematicDirectory;
}

std::filesystem::path RuntimeContext::resolveSchematicPath(std::string const& filename) const {
    return schematic::SchematicPathResolver(mSchematicDirectory).resolveExistingPath(filename);
}

std::vector<std::string> RuntimeContext::listAvailableSchematics() const {
    namespace fs = std::filesystem;

    std::vector<std::string> files;
    if (mSchematicDirectory.empty() || !fs::is_directory(mSchematicDirectory)) {
        return files;
    }

    for (auto const& entry : fs::directory_iterator(mSchematicDirectory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == ".mcstructure") {
            files.push_back(entry.path().filename().string());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

} // namespace levishematic::app
