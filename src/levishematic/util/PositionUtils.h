#pragma once
// ============================================================
// PositionUtils.h
// SubChunk 坐标编码 / 世界坐标工具
// ============================================================

#include "mc/world/level/BlockPos.h"

#include <cstdint>

namespace levishematic::util {

// ================================================================
// Key 编码工具
//
// 编码方案（uint64_t）：
//   bits[63:42] = (uint32_t)x           (22 bits)
//   bits[41:21] = (uint32_t)z & 0x1FFFFF (21 bits)
//   bits[20: 0] = (uint32_t)y & 0x1FFFFF (21 bits)
// ================================================================
inline uint64_t encodePosKey(int x, int y, int z) noexcept {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 42)
         | (static_cast<uint64_t>(static_cast<uint32_t>(z) & 0x1FFFFFu) << 21)
         | static_cast<uint64_t>(static_cast<uint32_t>(y) & 0x1FFFFFu);
}

inline uint64_t encodePosKey(const BlockPos& p) noexcept {
    return encodePosKey(p.x, p.y, p.z);
}

inline BlockPos decodePosKey(uint64_t key) noexcept {
    int x = static_cast<int>(key >> 42);
    int z = static_cast<int>((key >> 21) & 0x1FFFFFu);
    int y = static_cast<int>(key & 0x1FFFFFu);

    if (x & (1 << 21)) x |= ~0x3FFFFF;
    if (z & (1 << 20)) z |= ~0x1FFFFF;
    if (y & (1 << 20)) y |= ~0x1FFFFF;

    return {x, y, z};
}

// RenderChunkGeometry::mPosition 是 SubChunk 原点（已是16的整数倍）
// >>4 得 SubChunk 索引，用于 HashMap key
inline uint64_t encodeSubChunkKey(const BlockPos& origin) noexcept {
    int sx = origin.x >> 4;
    int sy = origin.y >> 4;
    int sz = origin.z >> 4;
    return (static_cast<uint64_t>(static_cast<uint32_t>(sx)) << 42)
         | (static_cast<uint64_t>(static_cast<uint32_t>(sz) & 0x1FFFFFu) << 21)
         | static_cast<uint64_t>(static_cast<uint32_t>(sy) & 0x1FFFFFu);
}

// 将任意世界坐标转为其所在 SubChunk 的 key
inline uint64_t subChunkKeyFromWorldPos(int wx, int wy, int wz) noexcept {
    auto floorDiv16 = [](int v) -> int {
        return v / 16 - (v % 16 != 0 && v < 0 ? 1 : 0);
    };
    BlockPos origin{floorDiv16(wx) * 16, floorDiv16(wy) * 16, floorDiv16(wz) * 16};
    return encodeSubChunkKey(origin);
}

// SubChunk 原点从世界坐标
inline BlockPos subChunkOrigin(int wx, int wy, int wz) noexcept {
    auto floorDiv16 = [](int v) -> int {
        return v / 16 - (v % 16 != 0 && v < 0 ? 1 : 0);
    };
    return {floorDiv16(wx) * 16, floorDiv16(wy) * 16, floorDiv16(wz) * 16};
}

} // namespace levishematic::util
