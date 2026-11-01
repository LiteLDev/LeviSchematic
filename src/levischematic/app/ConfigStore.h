#pragma once
#include "Config.h"
#include <filesystem>
#include <mutex>
#include <optional>

namespace levischematic::app {

class ConfigStore {
public:
    std::string initialize(std::filesystem::path path);

    [[nodiscard]] Config snapshot() const {
        std::scoped_lock lock(mMutex);
        return mCurrent;
    }

    std::string submit(Config value) {
        auto error = validateConfig(value);
        if (!error.empty()) return error;
        std::scoped_lock lock(mMutex);
        mPending = std::move(value);
        return {};
    }

    std::string reload();

    std::string save() const;

    std::optional<Config> applyPending() {
        std::scoped_lock lock(mMutex);
        if (!mPending) return {};
        mCurrent = std::move(*mPending);
        mPending.reset();
        return mCurrent;
    }

private:
    mutable std::mutex    mMutex;
    std::filesystem::path mPath;
    Config                mCurrent;
    std::optional<Config> mPending;
};

ConfigStore& configStore();
} 
