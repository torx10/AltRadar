#pragma once

#include "plugin_sdk/PluginSDK.h"

#include <cstddef>
#include <cstdint>

namespace RadarRender {

inline bool IsWalkableCell(const uint8_t* grid, size_t sizeBytes, int w, int h, int gx, int gy) {
    if (!grid || w <= 0 || h <= 0 || gx < 0 || gy < 0 || gx >= w || gy >= h) return false;
    const int rowStrideBytes = w / 2;
    const size_t byteIdx = static_cast<size_t>(gy) * static_cast<size_t>(rowStrideBytes)
                         + static_cast<size_t>(gx / 2);
    if (byteIdx >= sizeBytes) return false;
    const uint8_t cell = (gx & 1) ? (grid[byteIdx] >> 4) : (grid[byteIdx] & 0x0F);
    return cell != 0;
}

} // namespace RadarRender
