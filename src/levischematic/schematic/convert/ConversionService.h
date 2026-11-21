#pragma once
#include "levischematic/shared/result/Result.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace levischematic::convert {
struct ConversionStatus {
    uint64_t              id = 0;
    std::filesystem::path source;
    std::filesystem::path target;
    std::string           phase;
    std::string           detail;
    bool                  terminal = false;
};

class ConversionService {
public:
    ConversionService();
    ~ConversionService();
    ConversionService(ConversionService const&)            = delete;
    ConversionService& operator=(ConversionService const&) = delete;

    app::Result<uint64_t, std::string>
    request(std::filesystem::path source, std::filesystem::path target, bool overwrite);
    [[nodiscard]] std::optional<ConversionStatus> status() const;
    bool                                          cancel();
    void                                          shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
}
