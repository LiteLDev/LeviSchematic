#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace levischematic::schematic {
struct FileEntry {
    std::string           name;
    std::filesystem::path path;
    bool                  directory = false;
};

std::string            utf8(std::filesystem::path const& path);
bool                   isWithin(std::filesystem::path const& root, std::filesystem::path const& path);
bool                   validFileName(std::string const& name);
std::vector<FileEntry> listDirectory(std::filesystem::path const& directory, std::vector<std::string> const& suffixes);
std::string
withSuffix(std::string name, std::filesystem::path const& original, std::vector<std::string> const& suffixes);
std::string fileSuffix(std::filesystem::path const& path);

std::string
createDirectory(std::filesystem::path const& root, std::filesystem::path const& parent, std::string const& name);
std::string renameFile(std::filesystem::path const& root, std::filesystem::path const& file, std::string const& name);
std::string copyFile(std::filesystem::path const& root, std::filesystem::path const& file, std::string const& name);
std::string deleteFile(std::filesystem::path const& root, std::filesystem::path const& file);
} // namespace levischematic::schematic
