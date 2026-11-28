#pragma once

// Pathfinding over the PoE2 walkable grid, without the game: pure data, run off the UI thread.
//
// One distance field from the player gives the way to every target (landmarks, room targets, ...):
// a path is traced back down the field and smoothed by line of sight. Based on the PoE2 plugins
// InfernalVoooid/Wisps (distance wave, clearance penalty, no corner cutting) and Sikaka/POE2Radar
// (line-of-sight smoothing), both MIT. The field covers the whole map, so it uses integer step costs
// and a bucket queue (Dial's algorithm) instead of a binary heap.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace RadarPath {

struct Cell {
    int x = 0;
    int y = 0;
    bool operator==(const Cell& o) const { return x == o.x && y == o.y; }
};

// Decoded walkable grid: 0 = wall, 1..15 = walkable. PoE2 stores 4 bits per cell, two cells per
// byte (low nibble first), and the value grows with distance from walls (1..5 in practice).
struct Grid {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> cells;
    uint8_t maxValue = 1;

    bool Inside(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
    uint8_t At(int x, int y) const { return Inside(x, y) ? cells[static_cast<size_t>(y) * width + x] : 0; }
    bool Walkable(int x, int y) const { return At(x, y) != 0; }
};

inline Grid DecodeWalkable(const uint8_t* packed, size_t bytes, int width, int height) {
    Grid g;
    if (!packed || width <= 0 || height <= 0) return g;
    const size_t stride = static_cast<size_t>(width + 1) / 2;  // as RadarRender::IsWalkableCell (w / 2 for even w)
    g.width = width;
    g.height = height;
    g.cells.assign(static_cast<size_t>(width) * height, 0);
    for (int y = 0; y < height; ++y) {
        const size_t row = static_cast<size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            const size_t i = row + static_cast<size_t>(x / 2);
            if (i >= bytes) break;
            const uint8_t v = (x & 1) ? static_cast<uint8_t>(packed[i] >> 4) : static_cast<uint8_t>(packed[i] & 0x0F);
            g.cells[static_cast<size_t>(y) * width + x] = v;
            g.maxValue = std::max(g.maxValue, v);
        }
    }
    return g;
}

// The walkable cell nearest to (x, y) within `radius`, searched ring by ring (room centres and
// entities can sit on unwalkable cells).
inline bool NearestWalkable(const Grid& g, int x, int y, int radius, Cell& out) {
    if (g.Walkable(x, y)) {
        out = {x, y};
        return true;
    }
    for (int r = 1; r <= radius; ++r) {
        int bestD = -1;
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r || !g.Walkable(x + dx, y + dy)) continue;
                const int d = dx * dx + dy * dy;
                if (bestD < 0 || d < bestD) {
                    bestD = d;
                    out = {x + dx, y + dy};
                }
            }
        if (bestD >= 0) return true;
    }
    return false;
}

// Straight walk from a to b over walkable cells only, never squeezing diagonally past a wall corner.
inline bool HasClearLine(const Grid& g, Cell a, Cell b) {
    int x = a.x, y = a.y;
    const int dx = std::abs(b.x - x), dy = -std::abs(b.y - y);
    const int sx = x < b.x ? 1 : -1, sy = y < b.y ? 1 : -1;
    int err = dx + dy;
    while (true) {
        if (!g.Walkable(x, y)) return false;
        if (x == b.x && y == b.y) return true;
        const int e2 = 2 * err;
        const bool stepX = e2 >= dy, stepY = e2 <= dx;
        if (stepX && stepY && (!g.Walkable(x + sx, y) || !g.Walkable(x, y + sy))) return false;
        if (stepX) {
            err += dy;
            x += sx;
        }
        if (stepY) {
            err += dx;
            y += sy;
        }
    }
}

// Walking cost from one start cell to every reachable cell. A step costs 10 (straight) or 14
// (diagonal), times a lane factor from 1 (open ground) to 3 (hugging a wall), so paths keep to the
// middle of corridors the way players walk.
class DistanceField {
public:
    static constexpr uint32_t kUnreached = UINT32_MAX;

    int width = 0;
    int height = 0;
    Cell start;
    std::vector<uint32_t> cost;

    bool Ready() const { return width > 0 && !cost.empty(); }
    uint32_t CostAt(int x, int y) const {
        return (x >= 0 && y >= 0 && x < width && y < height) ? cost[static_cast<size_t>(y) * width + x] : kUnreached;
    }

    // With `targets`, stops once each reachable target's cost is final (its bucket came up). Cells
    // past that frontier stay unreached, so near targets cost a fraction of the whole map; every
    // cell on a target's path is final, so TracePath still works.
    void Build(const Grid& g, Cell from, const std::vector<Cell>* targets = nullptr) {
        width = g.width;
        height = g.height;
        start = from;
        cost.assign(static_cast<size_t>(width) * height, kUnreached);
        if (!g.Walkable(from.x, from.y)) return;

        // Step costs per cell value: lane = 1 + 2 * (1 - value / maxValue), as in Wisps.
        uint32_t straight[16], diagonal[16];
        uint32_t maxStep = 1;
        for (int v = 0; v < 16; ++v) {
            const float c = static_cast<float>(std::clamp<int>(v, 1, g.maxValue));
            const float lane = 1.f + 2.f * (1.f - c / static_cast<float>(g.maxValue));
            straight[v] = static_cast<uint32_t>(std::lround(10.f * lane));
            diagonal[v] = static_cast<uint32_t>(std::lround(14.f * lane));
            maxStep = std::max(maxStep, diagonal[v]);
        }

        // Dial's algorithm: every pending cost lies within maxStep of the current one, so a ring of
        // maxStep + 1 buckets holds the queue.
        std::vector<std::vector<uint32_t>> buckets(maxStep + 1);
        const auto index = [&](int x, int y) { return static_cast<uint32_t>(y * width + x); };
        cost[index(from.x, from.y)] = 0;
        buckets[0].push_back(index(from.x, from.y));
        size_t pending = 1;
        std::vector<uint32_t> waiting;  // target cells not final yet
        if (targets)
            for (const Cell& t : *targets)
                if (g.Walkable(t.x, t.y)) waiting.push_back(index(t.x, t.y));
        static constexpr int kDx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
        static constexpr int kDy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        for (uint32_t d = 0; pending > 0; ++d) {
            if (targets) {
                waiting.erase(std::remove_if(waiting.begin(), waiting.end(), [&](uint32_t t) { return cost[t] < d; }),
                              waiting.end());
                if (waiting.empty()) break;
            }
            auto& bucket = buckets[d % buckets.size()];
            // Steps cost 10..maxStep, so nothing lands back in the bucket being walked.
            for (size_t i = 0; i < bucket.size(); ++i) {
                const uint32_t cur = bucket[i];
                --pending;
                if (cost[cur] != d) continue;  // a stale entry: improved since it was queued
                const int cx = static_cast<int>(cur % width), cy = static_cast<int>(cur / width);
                const uint8_t v = g.cells[cur];
                for (int k = 0; k < 8; ++k) {
                    const int nx = cx + kDx[k], ny = cy + kDy[k];
                    if (!g.Walkable(nx, ny)) continue;
                    const bool diag = (k & 1) != 0;
                    if (diag && (!g.Walkable(nx, cy) || !g.Walkable(cx, ny))) continue;  // no corner cutting
                    const uint32_t nd = d + (diag ? diagonal[v] : straight[v]);
                    const uint32_t ni = index(nx, ny);
                    if (nd >= cost[ni]) continue;
                    cost[ni] = nd;
                    buckets[nd % buckets.size()].push_back(ni);
                    ++pending;
                }
            }
            bucket.clear();
        }
    }
};

// The way from the field's start to `target` (start first), smoothed by line of sight. Empty when
// the target cannot be reached. maxSegment caps a smoothed segment so lines do not cut long
// shortcuts across open ground the player would not take.
inline std::vector<Cell> TracePath(const Grid& g, const DistanceField& f, Cell target, int maxSegment = 60) {
    std::vector<Cell> raw;
    if (!f.Ready() || f.CostAt(target.x, target.y) == DistanceField::kUnreached) return raw;
    Cell c = target;
    raw.push_back(c);
    while (!(c == f.start)) {  // walk downhill; every reached cell except the start has a lower neighbour
        Cell best = c;
        uint32_t bestCost = f.CostAt(c.x, c.y);
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                // Only steps the field could take: a diagonal must not squeeze past a wall corner.
                if (dx && dy && (!g.Walkable(c.x + dx, c.y) || !g.Walkable(c.x, c.y + dy))) continue;
                const uint32_t n = f.CostAt(c.x + dx, c.y + dy);
                if (n < bestCost) {
                    bestCost = n;
                    best = {c.x + dx, c.y + dy};
                }
            }
        if (best == c) break;
        c = best;
        raw.push_back(c);
    }
    std::reverse(raw.begin(), raw.end());

    std::vector<Cell> out{raw.front()};
    for (size_t i = 0; i + 1 < raw.size();) {
        size_t next = i + 1;
        const size_t limit = std::min(raw.size() - 1, i + static_cast<size_t>(maxSegment));
        for (size_t j = limit; j > i + 1; --j)
            if (HasClearLine(g, raw[i], raw[j])) {
                next = j;
                break;
            }
        out.push_back(raw[next]);
        i = next;
    }
    return out;
}

} // namespace RadarPath
