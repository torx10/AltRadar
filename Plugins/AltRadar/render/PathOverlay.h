#pragma once

// PathFinder on the radar: hands the area's walkable grid and the wanted targets to the
// RadarPath::PathService worker, and draws its latest paths each frame (a few hundred points at
// most, so DrawUI only ever pays for drawing).

#include "MapProjection.h"
#include "data/PathService.h"
#include "data/RadarConfig.h"
#include "plugin_sdk/PluginSDK.h"

#include <imgui.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <vector>

namespace RadarRender {

struct PathTargetSpec {
    uint64_t key = 0;  // stable while the target exists, so a moving target is not a new one
    float gridX = 0.f;
    float gridY = 0.f;
    ImU32 color = IM_COL32_WHITE;
};

class PathOverlay {
public:
    void Release() {
        service.reset();  // joins the worker
        gate.Reset();
        gridData = nullptr;
        targets.clear();
    }

    // Each frame: a new area's grid goes to the worker; a request goes when RequestGate allows.
    void Update(const PluginSDK::WalkableGridHandle& grid, const PluginSDK::Snapshot& snap,
                std::vector<PathTargetSpec> wanted) {
        targets = std::move(wanted);
        if (targets.empty() || !grid.Valid() || !snap.Player.IsValid) return;
        if (!service) service = std::make_unique<RadarPath::PathService>();
        if (grid.Data() != gridData) {
            gridData = grid.Data();
            service->SetGrid(std::vector<uint8_t>(grid.Data(), grid.Data() + grid.SizeBytes()), grid.Width(),
                             grid.Height(), ++gridId);
            gate.Reset();
        }
        std::vector<RadarPath::PathTarget> request;
        request.reserve(targets.size());
        for (const auto& t : targets) request.push_back({t.key, ToCell(t.gridX, t.gridY)});
        const auto player = ToCell(snap.Player.GridPositionX, snap.Player.GridPositionY);
        if (gate.ShouldSend(player, request, gridId, std::chrono::steady_clock::now()))
            service->Request(player, std::move(request));
    }

    void Draw(ImDrawList* dl, PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
              const RadarData::RadarConfig& cfg, const MapLayerProjection* largeMapProj) const {
        if (!dl || !ctx || !service || targets.empty() || !snap.Player.IsValid) return;
        const auto result = service->Latest();
        if (!result || result->gridId != gridId) return;
        const bool large = largeMapProj && largeMapProj->valid && snap.LargeMap.IsVisible;
        auto project = [&](float gx, float gy, ImVec2& out) {
            const ProjectedScreen s =
                large ? ProjectGridLargeMapLayer(*largeMapProj, ctx, snap, gx, gy, 0.f, MapLayerSubject::Terrain)
                      : ProjectGridToMiniMapScreen(ctx, snap, gx, gy, TerrainHeightAtGrid(ctx, gx, gy), 6.f, false);
            out = ImVec2(s.sx, s.sy);
            return s.valid;
        };
        const float thickness = large ? 2.5f : 2.f;
        const float px = snap.Player.GridPositionX, py = snap.Player.GridPositionY;

        for (const auto& path : result->paths) {
            const PathTargetSpec* spec = nullptr;
            for (const auto& t : targets)
                if (t.key == path.key) spec = &t;
            if (!spec) continue;  // no longer wanted
            const ImU32 color = cfg.PathMulticolor ? PaletteColor(path.key) : spec->color;

            // Join the live player to the path, past the waypoints already walked.
            const auto& pts = path.points;
            size_t nearest = 0;
            float best = 1e30f;
            for (size_t i = 0; i < pts.size(); ++i) {
                const float dx = pts[i].x - px, dy = pts[i].y - py, d = dx * dx + dy * dy;
                if (d < best) {
                    best = d;
                    nearest = i;
                }
            }
            ImVec2 prev;
            bool prevValid = project(px, py, prev);
            for (size_t i = nearest + 1; i <= pts.size(); ++i) {
                const bool last = i == pts.size();
                ImVec2 cur;  // the last point is the target itself, not its snapped cell
                const bool valid = last ? project(spec->gridX, spec->gridY, cur)
                                        : project(static_cast<float>(pts[i].x), static_cast<float>(pts[i].y), cur);
                if (prevValid && valid) {
                    dl->AddLine(prev, cur, IM_COL32(0, 0, 0, 150), thickness + 2.f);
                    dl->AddLine(prev, cur, color, thickness);
                } else if (prevValid && !large) {
                    break;  // left the minimap: the rest is off it too
                }
                prev = cur;
                prevValid = valid;
            }
        }
    }

    static ImU32 PaletteColor(uint64_t key) {
        static constexpr ImU32 kColors[] = {IM_COL32(255, 90, 90, 255),  IM_COL32(90, 200, 255, 255),
                                            IM_COL32(120, 255, 120, 255), IM_COL32(255, 200, 60, 255),
                                            IM_COL32(220, 120, 255, 255), IM_COL32(255, 140, 200, 255),
                                            IM_COL32(80, 255, 220, 255),  IM_COL32(255, 255, 255, 255)};
        return kColors[(key * 0x9E3779B97F4A7C15ull) >> 61];
    }

private:
    static RadarPath::Cell ToCell(float gx, float gy) {
        return {static_cast<int>(std::lround(gx)), static_cast<int>(std::lround(gy))};
    }

    std::unique_ptr<RadarPath::PathService> service;
    RadarPath::RequestGate gate;
    const uint8_t* gridData = nullptr;
    uint64_t gridId = 0;
    std::vector<PathTargetSpec> targets;
};

} // namespace RadarRender
