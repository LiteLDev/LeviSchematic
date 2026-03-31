#pragma once

#include <utility>
#include <variant>

namespace levishematic::app {

template <typename T, typename E>
class Result {
public:
    static Result success(T value) {
        return Result(std::in_place_index<0>, std::move(value));
    }

    static Result failure(E error) {
        return Result(std::in_place_index<1>, std::move(error));
    }

    [[nodiscard]] bool hasValue() const {
        return std::holds_alternative<T>(mData);
    }

    explicit operator bool() const {
        return hasValue();
    }

    T& value() & {
        return std::get<T>(mData);
    }

    T const& value() const& {
        return std::get<T>(mData);
    }

    T&& value() && {
        return std::get<T>(std::move(mData));
    }

    E& error() & {
        return std::get<E>(mData);
    }

    E const& error() const& {
        return std::get<E>(mData);
    }

private:
    template <typename... Args>
    Result(std::in_place_index_t<0>, Args&&... args) : mData(std::in_place_index<0>, std::forward<Args>(args)...) {}

    template <typename... Args>
    Result(std::in_place_index_t<1>, Args&&... args) : mData(std::in_place_index<1>, std::forward<Args>(args)...) {}

    std::variant<T, E> mData;
};

template <typename E>
using StatusResult = Result<std::monostate, E>;

template <typename E>
inline StatusResult<E> ok() {
    return StatusResult<E>::success(std::monostate{});
}

} // namespace levishematic::app
