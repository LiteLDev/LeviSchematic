#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace levischematic::worldedit {
using Position = std::array<int, 3>;
enum class Operation { Paste, GridPaste, Fill, Replace, Delete, Clone, Move };
enum class ReplacePolicy { TargetAir, SourceNonAir, All };
enum class Phase {
    Idle,
    Preparing,
    Ready,
    Running,
    Waiting,
    Paused,
    Verifying,
    Cleaning,
    Complete,
    Partial,
    Failed,
    Cancelled
};
enum class Outcome { Waiting, InFlight, Observed, Verified, Unsupported, Rejected };
inline bool terminal(Phase p) {
    return p == Phase::Idle || p == Phase::Complete || p == Phase::Partial || p == Phase::Failed
        || p == Phase::Cancelled;
}
inline char const* phaseName(Phase p) {
    constexpr char const* names[]{
        "Idle",
        "Preparing",
        "Ready",
        "Running",
        "Waiting",
        "Paused",
        "Verifying",
        "Cleaning",
        "Complete",
        "Partial",
        "Failed",
        "Cancelled"
    };
    return names[static_cast<int>(p)];
}
inline bool shouldWrite(ReplacePolicy policy, bool sourceAir, bool targetAir) {
    return policy == ReplacePolicy::All || (policy == ReplacePolicy::TargetAir ? targetAir && !sourceAir : !sourceAir);
}
inline std::optional<Position> translate(Position p, Position delta) {
    for (size_t i = 0; i < 3; ++i) {
        auto v = int64_t(p[i]) + delta[i];
        if (v < INT32_MIN || v > INT32_MAX) return {};
        p[i] = static_cast<int>(v);
    }
    return p;
}
struct Progress {
    uint64_t    id       = 0;
    Phase       phase    = Phase::Idle;
    size_t      prepared = 0, total = 0, observed = 0, verified = 0, skipped = 0, rejected = 0;
    std::string detail;
    bool        terminal() const { return worldedit::terminal(phase); }
};

class Task {
    std::vector<Outcome>  mCells;
    size_t                mCursor = 0;
    std::optional<size_t> mFlight;
    bool                  mMove   = false;
    Phase                 mResume = Phase::Running;

public:
    Progress progress;
    void     reset(uint64_t id, size_t count, bool move) {
        mCells.assign(count, Outcome::Waiting);
        mCursor = 0;
        mMove   = move;
        mFlight.reset();
        progress       = {};
        progress.id    = id;
        progress.total = count;
        progress.phase = Phase::Ready;
    }
    void start() {
        if (progress.phase == Phase::Ready) progress.phase = Phase::Running;
    }
    void pause() {
        if (!terminal(progress.phase) && progress.phase != Phase::Paused && progress.phase != Phase::Preparing) {
            mResume        = progress.phase;
            progress.phase = Phase::Paused;
        }
    }
    void resume() {
        if (progress.phase == Phase::Paused) progress.phase = mResume;
    }
    void cancel(std::string reason = "Cancelled; existing world changes retained.") {
        if (!terminal(progress.phase)) {
            progress.phase  = Phase::Cancelled;
            progress.detail = std::move(reason);
        }
    }
    Outcome outcome(size_t i) const { return mCells.at(i); }
    void    record(size_t i, Outcome value) {
        count(mCells.at(i), false);
        mCells[i] = value;
        count(value, true);
        if (value == Outcome::InFlight) mFlight = i;
        else if (mFlight == i) mFlight.reset();
    }
    std::optional<size_t> next() {
        if (progress.phase != Phase::Running && progress.phase != Phase::Waiting) return {};
        
        if (mFlight) return mFlight;
        for (size_t n = 0; n < mCells.size(); ++n) {
            auto i = mCursor++ % mCells.size();
            if (mCells[i] == Outcome::Waiting) return i;
        }
        progress.phase =
            mMove ? Phase::Verifying : (progress.skipped || progress.rejected ? Phase::Partial : Phase::Complete);
        return {};
    }
    bool canClean() const {
        return mMove && progress.phase == Phase::Verifying && !progress.skipped && !progress.rejected
            && progress.verified == progress.total;
    }

private:
    void count(Outcome value, bool add) {
        auto adjust = [add](size_t& v) {
            if (add) ++v;
            else --v;
        };
        if (value == Outcome::Observed || value == Outcome::Verified) adjust(progress.observed);
        if (value == Outcome::Verified) adjust(progress.verified);
        if (value == Outcome::Unsupported) adjust(progress.skipped);
        if (value == Outcome::Rejected) adjust(progress.rejected);
    }
};
} 
