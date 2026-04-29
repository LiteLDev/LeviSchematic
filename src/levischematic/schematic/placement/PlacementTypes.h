#pragma once

#include "levischematic/schematic/placement/PlacementModel.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace levischematic::placement {

struct LoadPlacementError {
    enum class Code {
        FileNotFound,
        FileReadFailed,
        NbtParseFailed,
        TemplateLoadFailed,
        EmptyStructure,
        RegistryError,
    };

    Code        code{};
    std::string detail;

    [[nodiscard]] std::string describe(std::string_view target) const;
};

template <typename T, typename E>
class Expected {
public:
    static Expected success(T value) {
        Expected result;
        result.mValue.emplace(std::move(value));
        return result;
    }

    static Expected failure(E error) {
        Expected result;
        result.mError.emplace(std::move(error));
        return result;
    }

    [[nodiscard]] bool hasValue() const { return mValue.has_value(); }
    explicit operator bool() const { return hasValue(); }

    T&       value() & { return *mValue; }
    T const& value() const& { return *mValue; }
    T&&      value() && { return std::move(*mValue); }

    E const& error() const { return *mError; }

private:
    std::optional<T> mValue;
    std::optional<E> mError;
};

using LoadAssetResult = Expected<std::shared_ptr<const SchematicAsset>, LoadPlacementError>;

struct LoadPlacementResult {
    std::optional<PlacementId>        id;
    std::filesystem::path             resolvedPath;
    std::optional<LoadPlacementError> error;

    explicit operator bool() const { return id.has_value(); }
};

} // namespace levischematic::placement
