#pragma once

#include "levishematic/schematic/placement/SchematicPlacement.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace levishematic::placement {

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

    std::string describe(std::string_view target) const;
};

template <typename T, typename E>
class Result {
public:
    static Result success(T value) {
        Result result;
        result.mValue.emplace(std::move(value));
        return result;
    }

    static Result failure(E error) {
        Result result;
        result.mError.emplace(std::move(error));
        return result;
    }

    bool hasValue() const { return mValue.has_value(); }
    explicit operator bool() const { return hasValue(); }

    T& value() & { return *mValue; }
    const T& value() const& { return *mValue; }
    T&& value() && { return std::move(*mValue); }

    const E& error() const { return *mError; }

private:
    std::optional<T> mValue;
    std::optional<E> mError;
};

using LoadPlacementDataResult = Result<SchematicPlacement, LoadPlacementError>;

struct LoadPlacementResult {
    SchematicPlacement::Id            id = 0;
    std::filesystem::path             resolvedPath;
    std::optional<LoadPlacementError> error;

    explicit operator bool() const { return id != 0; }
};

} // namespace levishematic::placement

