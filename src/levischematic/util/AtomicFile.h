#pragma once
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace levischematic::util {

inline std::filesystem::path atomicTemporaryPath(std::filesystem::path path) {
    path += ".tmp";
    return path;
}
inline void writeAtomicTemporary(std::filesystem::path const& path, std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    auto          temporary = atomicTemporaryPath(path);
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output || !output.write(bytes.data(), static_cast<std::streamsize>(bytes.size())) || !output.flush())
        throw std::runtime_error("Cannot write temporary file.");
}
inline void commitAtomic(std::filesystem::path const& path, bool overwrite = true) {
    auto temporary = atomicTemporaryPath(path);
    auto flags     = MOVEFILE_WRITE_THROUGH | (overwrite ? MOVEFILE_REPLACE_EXISTING : 0);
    if (!MoveFileExW(temporary.c_str(), path.c_str(), flags))
        throw std::runtime_error("Cannot commit file (target exists, locked or disk unavailable).");
}
inline void writeAtomic(std::filesystem::path const& path, std::string_view bytes, bool overwrite = true) {
    try {
        writeAtomicTemporary(path, bytes);
        commitAtomic(path, overwrite);
    } catch (...) {
        std::error_code ec;
        std::filesystem::remove(atomicTemporaryPath(path), ec);
        throw;
    }
}
} 
