#pragma once

// Runs RadarPath on its own thread so DrawUI never waits for it (POE2Fixer disables a plugin whose
// DrawUI stays over 50 ms). The UI thread hands over a copy of the area's packed walkable grid and,
// whenever the player or the targets change enough, the latest request; the worker builds one
// distance field from the player and traces a path to every target. Only the newest request is
// kept, so a moving player never queues up stale work. No game calls happen on the worker.

#include "PathField.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace RadarPath {

struct PathTarget {
    uint64_t key = 0;  // stable per target, so the drawing side can keep its colour
    Cell cell;
};

struct PathResult {
    uint64_t gridId = 0;
    Cell from;  // the player cell the field was built from
    struct Path {
        uint64_t key = 0;
        std::vector<Cell> points;  // from the player to the target
    };
    std::vector<Path> paths;
};

// When DrawUI sends a new request. A changed area or target set goes at once; a target that moved
// (a monster, a runeshape) at most every 250 ms; the player walking away from where the field was
// built at most every 500 ms. In between, the drawing joins the live player to the existing path.
class RequestGate {
public:
    static constexpr int kPlayerMoveCells = 25;
    static constexpr int kTargetMoveCells = 8;

    bool ShouldSend(Cell player, const std::vector<PathTarget>& targets, uint64_t gridId,
                    std::chrono::steady_clock::time_point now) {
        const auto since = now - m_sentAt;
        bool send = !m_sent || gridId != m_gridId || targets.size() != m_targets.size();
        for (size_t i = 0; !send && i < targets.size(); ++i) send = targets[i].key != m_targets[i].key;
        bool targetMoved = false;
        for (size_t i = 0; !send && i < targets.size(); ++i)
            targetMoved |= Far(targets[i].cell, m_targets[i].cell, kTargetMoveCells);
        send = send || (targetMoved && since >= std::chrono::milliseconds(250))
               || (Far(player, m_player, kPlayerMoveCells) && since >= std::chrono::milliseconds(500));
        if (!send) return false;
        m_sent = true;
        m_sentAt = now;
        m_player = player;
        m_targets = targets;
        m_gridId = gridId;
        return true;
    }

    void Reset() { m_sent = false; }

private:
    static bool Far(Cell a, Cell b, int cells) {
        const int dx = a.x - b.x, dy = a.y - b.y;
        return dx * dx + dy * dy > cells * cells;
    }
    bool m_sent = false;
    std::chrono::steady_clock::time_point m_sentAt{};
    Cell m_player;
    std::vector<PathTarget> m_targets;
    uint64_t m_gridId = 0;
};

class PathService {
public:
    // How far a request may snap the player or a target onto walkable ground (room centres,
    // entities standing on doodads).
    static constexpr int kSnapRadius = 40;

    PathService() : m_worker([this] { Run(); }) {}
    ~PathService() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_wake.notify_all();
        m_worker.join();
    }
    PathService(const PathService&) = delete;
    PathService& operator=(const PathService&) = delete;

    // A new area's grid (4-bit packed, as WalkableGridHandle). Decoded on the worker.
    void SetGrid(std::vector<uint8_t> packed, int width, int height, uint64_t gridId) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pendingGrid = PendingGrid{std::move(packed), width, height, gridId};
            m_lastGridId = gridId;
            m_result.reset();  // paths of the old area are meaningless now
        }
        m_wake.notify_all();
    }

    void Request(Cell player, std::vector<PathTarget> targets) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_request = Req{player, std::move(targets)};
        }
        m_wake.notify_all();
    }

    std::shared_ptr<const PathResult> Latest() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_result;
    }

    bool Busy() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_working || m_request.has_value() || m_pendingGrid.has_value();
    }

private:
    struct PendingGrid {
        std::vector<uint8_t> packed;
        int width = 0;
        int height = 0;
        uint64_t id = 0;
    };
    struct Req {
        Cell player;
        std::vector<PathTarget> targets;
    };

    void Run() {
        Grid grid;
        uint64_t gridId = 0;
        DistanceField field;
        while (true) {
            std::optional<PendingGrid> newGrid;
            std::optional<Req> req;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_working = false;
                m_wake.wait(lock, [&] { return m_stop || m_pendingGrid || m_request; });
                if (m_stop) return;
                newGrid.swap(m_pendingGrid);
                req.swap(m_request);
                m_working = true;
            }
            if (newGrid) {
                grid = DecodeWalkable(newGrid->packed.data(), newGrid->packed.size(), newGrid->width, newGrid->height);
                gridId = newGrid->id;
            }
            if (!req || grid.width == 0) continue;

            auto result = std::make_shared<PathResult>();
            result->gridId = gridId;
            if (NearestWalkable(grid, req->player.x, req->player.y, kSnapRadius, result->from)) {
                std::vector<PathTarget> snapped;
                std::vector<Cell> cells;
                for (const auto& t : req->targets) {
                    Cell c;
                    if (!NearestWalkable(grid, t.cell.x, t.cell.y, kSnapRadius, c)) continue;
                    snapped.push_back({t.key, c});
                    cells.push_back(c);
                }
                field.Build(grid, result->from, &cells);
                for (const auto& t : snapped) {
                    auto points = TracePath(grid, field, t.cell);
                    if (points.size() >= 2) result->paths.push_back({t.key, std::move(points)});
                }
            }
            std::lock_guard<std::mutex> lock(m_mutex);
            if (gridId == m_lastGridId) m_result = std::move(result);  // drop work for an older area
        }
    }

    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    bool m_stop = false;
    bool m_working = false;
    std::optional<PendingGrid> m_pendingGrid;
    std::optional<Req> m_request;
    std::shared_ptr<const PathResult> m_result;
    uint64_t m_lastGridId = 0;  // the grid the UI side set last
    std::thread m_worker;  // last: starts after every other member exists
};

} // namespace RadarPath
