#pragma once
#include "levischematic/util/VoxelRay.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace levischematic::survival {
using Position = std::array<int, 3>;
using Vector   = std::array<double, 3>;
using util::traceVoxels;
struct ItemKey {
    std::string name;
    int         aux                              = 0;
    bool        operator==(ItemKey const&) const = default;
    bool        operator<(ItemKey const& rhs) const { return name == rhs.name ? aux < rhs.aux : name < rhs.name; }
};
struct MaterialCounts {
    uint64_t total = 0, missing = 0, wrongBlock = 0, wrongState = 0, unknown = 0, available = 0;
    uint64_t remaining() const { return missing + wrongBlock + wrongState; }
    uint64_t toGather() const { return remaining() > available ? remaining() - available : 0; }
};
inline std::string csvCell(std::string_view value) {
    std::string result = "\"";
    for (char c : value) {
        if (c == '"') result += '"';
        result += c;
    }
    return result + '"';
}
enum class Phase { Idle, Selecting, WaitingInventory, Ready, WaitingWorld, Failed };

struct Operation {
    Phase    phase   = Phase::Idle;
    uint64_t started = 0;
    void     begin(uint64_t now) {
        phase   = Phase::Selecting;
        started = now;
    }
    bool expired(uint64_t now, uint64_t timeout = 2000) const {
        return phase != Phase::Idle && now - started >= timeout;
    }
    void reset() { *this = {}; }
};
} 
