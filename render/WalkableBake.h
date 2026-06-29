#pragma once

#include "WalkableGridView.h"
#include "data/RadarConfig.h"
#include "sdk/PluginSDK.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace RadarRender {

struct WalkableBake {
    struct BoundarySegment {
        uint16_t x0 = 0;
        uint16_t y0 = 0;
        uint16_t x1 = 0;
        uint16_t y1 = 0;
    };

    int                       width = 0;
    int                       height = 0;
    const uint8_t*            sourcePtr = nullptr;
    std::vector<uint8_t>      walkableMask;
    std::vector<BoundarySegment> boundarySegments;
    bool                      valid = false;

    void Clear() {
        width = 0;
        height = 0;
        sourcePtr = nullptr;
        walkableMask.clear();
        boundarySegments.clear();
        valid = false;
    }

    size_t CellCount() const {
        return (width > 0 && height > 0)
                   ? static_cast<size_t>(width) * static_cast<size_t>(height)
                   : 0;
    }

    bool IsWalkable(int gx, int gy) const {
        if (gx < 0 || gy < 0 || gx >= width || gy >= height) return false;
        const size_t idx =
            static_cast<size_t>(gy) * static_cast<size_t>(width) + static_cast<size_t>(gx);
        return idx < walkableMask.size() && walkableMask[idx] != 0;
    }

    static bool IsBoundaryCell(const std::vector<uint8_t>& mask, int w, int h, int gx, int gy) {
        const size_t idx =
            static_cast<size_t>(gy) * static_cast<size_t>(w) + static_cast<size_t>(gx);
        if (idx >= mask.size() || mask[idx] == 0) return false;

        for (int dy = -1; dy <= 1; ++dy) {
            const int ny = gy + dy;
            if (ny < 0 || ny >= h) return true;
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                const int nx = gx + dx;
                if (nx < 0 || nx >= w) return true;
                const size_t nidx = static_cast<size_t>(ny) * static_cast<size_t>(w)
                                  + static_cast<size_t>(nx);
                if (nidx >= mask.size() || mask[nidx] == 0) return true;
            }
        }

        return false;
    }

    void Rebuild(PluginSDK::Context* ctx, const PluginSDK::WalkableGridHandle& grid,
                 const RadarData::RadarConfig& cfg) {
        (void)ctx;
        (void)cfg;

        Clear();
        if (!grid.Valid()) return;

        const uint8_t* data = grid.Data();
        const int w = grid.Width();
        const int h = grid.Height();
        const size_t sizeBytes = grid.SizeBytes();
        if (!data || w <= 0 || h <= 0) return;

        width = w;
        height = h;
        sourcePtr = data;

        const size_t cellCount = CellCount();
        walkableMask.assign(cellCount, 0);
        for (int gy = 0; gy < h; ++gy) {
            for (int gx = 0; gx < w; ++gx) {
                if (!IsWalkableCell(data, sizeBytes, w, h, gx, gy)) continue;
                const size_t idx = static_cast<size_t>(gy) * static_cast<size_t>(w)
                                 + static_cast<size_t>(gx);
                walkableMask[idx] = 1;
            }
        }

        boundarySegments.clear();
        bool anyWalkable = false;
        for (int gy = 0; gy < h; ++gy) {
            for (int gx = 0; gx < w; ++gx) {
                const size_t idx = static_cast<size_t>(gy) * static_cast<size_t>(w)
                                 + static_cast<size_t>(gx);
                if (walkableMask[idx] == 0) continue;
                anyWalkable = true;
                if (!IsBoundaryCell(walkableMask, w, h, gx, gy)) continue;

                if (!IsWalkable(gx, gy - 1))
                    boundarySegments.push_back(BoundarySegment{
                        static_cast<uint16_t>(gx),
                        static_cast<uint16_t>(gy),
                        static_cast<uint16_t>(gx + 1),
                        static_cast<uint16_t>(gy),
                    });
                if (!IsWalkable(gx + 1, gy))
                    boundarySegments.push_back(BoundarySegment{
                        static_cast<uint16_t>(gx + 1),
                        static_cast<uint16_t>(gy),
                        static_cast<uint16_t>(gx + 1),
                        static_cast<uint16_t>(gy + 1),
                    });
                if (!IsWalkable(gx, gy + 1))
                    boundarySegments.push_back(BoundarySegment{
                        static_cast<uint16_t>(gx),
                        static_cast<uint16_t>(gy + 1),
                        static_cast<uint16_t>(gx + 1),
                        static_cast<uint16_t>(gy + 1),
                    });
                if (!IsWalkable(gx - 1, gy))
                    boundarySegments.push_back(BoundarySegment{
                        static_cast<uint16_t>(gx),
                        static_cast<uint16_t>(gy),
                        static_cast<uint16_t>(gx),
                        static_cast<uint16_t>(gy + 1),
                    });
            }
        }

        valid = anyWalkable;
    }
};

} // namespace RadarRender
