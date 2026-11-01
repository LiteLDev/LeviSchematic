#pragma once

#include <chrono>
#include <cstddef>
#include <optional>

namespace levischematic::verifier {

class VerificationPollSchedule {
public:
    using Clock = std::chrono::steady_clock;

    void reset(Clock::time_point now = Clock::now()) {
        mCursor    = 0;
        mNextSweep = now + std::chrono::seconds{1};
    }

    std::optional<size_t> next(size_t count, Clock::time_point now) {
        if (count == 0 || now < mNextSweep) return {};
        if (mCursor >= count) mCursor = 0;
        auto index = mCursor++;
        if (mCursor == count) reset(now);
        return index;
    }

private:
    size_t            mCursor = 0;
    Clock::time_point mNextSweep{};
};

} 
