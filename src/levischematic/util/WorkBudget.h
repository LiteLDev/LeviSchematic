#pragma once
#include <chrono>

namespace levischematic::util {

class WorkBudget {
public:
    using Clock = std::chrono::steady_clock;

    explicit WorkBudget(double milliseconds)
    : mStart(Clock::now()),
      mEnd(
          mStart + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double, std::milli>(milliseconds))
      ) {}

    [[nodiscard]] bool available() const { return Clock::now() < mEnd; }

    [[nodiscard]] double elapsedMs() const {
        return std::chrono::duration<double, std::milli>(Clock::now() - mStart).count();
    }

private:
    Clock::time_point mStart;
    Clock::time_point mEnd;
};
} 
