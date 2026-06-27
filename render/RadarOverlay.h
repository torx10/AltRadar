#pragma once

#include "IconAtlas.h"
#include "MapProjection.h"
#include "TerrainTexture.h"
#include "perf/AreaCache.h"
#include "data/RadarConfig.h"
#include "data/RadarLog.h"
#include "data/TargetDatabase.h"
#include "data/IconTables.h"
#include "sdk/PluginSDK.h"

#include <algorithm>
#include <filesystem>
#include <imgui.h>
#include <vector>

namespace RadarRender {

inline float ClampTerrainGrid(float value, int extent) {
    if (extent <= 0) return 0.f;
    const float maxValue = static_cast<float>(extent - 1);
    return std::clamp(value, 0.f, maxValue);
}

inline bool ProjectTerrainGridCorner(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                                     bool useLargeMap, int gridWidth, int gridHeight, float gx,
                                     float gy, float& sx, float& sy) {
    if (!ctx) return false;

    const float sampleX = ClampTerrainGrid(gx, gridWidth);
    const float sampleY = ClampTerrainGrid(gy, gridHeight);
    const float terrainZ = TerrainHeightAtGrid(ctx, sampleX, sampleY);

    if (useLargeMap) {
        if (ctx->Render.GridToLargeMap(gx, gy, terrainZ, sx, sy)) return true;
        const auto t = ctx->Render.GetLargeMapTransform();
        return ProjectWithMapTransform(t, gx, gy, terrainZ, WorldToGridScale(snap, ctx), sx, sy);
    }

    float rawX = 0.f;
    float rawY = 0.f;
    if (ctx->Render.GridToMiniMap(gx, gy, terrainZ, rawX, rawY)
        && !MiniMapGridLooksLikeViewport(ctx, snap, rawX, rawY)) {
        sx = rawX;
        sy = rawY;
        return true;
    }

    const auto t = ctx->Render.GetMiniMapTransform();
    return ProjectWithMapTransform(t, gx, gy, terrainZ, WorldToGridScale(snap, ctx), sx, sy);
}

inline bool ProjectTerrainMiniMapCornerSafe(PluginSDK::Context* ctx,
                                            const PluginSDK::Snapshot& snap, int gridWidth,
                                            int gridHeight, float gx, float gy, float& sx,
                                            float& sy) {
    if (!ctx) return false;
    const float sampleX = ClampTerrainGrid(gx, gridWidth);
    const float sampleY = ClampTerrainGrid(gy, gridHeight);
    const float terrainZ = TerrainHeightAtGrid(ctx, sampleX, sampleY);
    float rawX = 0.f;
    float rawY = 0.f;
    const bool gridOk = ctx->Render.GridToMiniMap(gx, gy, terrainZ, rawX, rawY);
    if (gridOk && !MiniMapGridLooksLikeViewport(ctx, snap, rawX, rawY)) {
        sx = rawX;
        sy = rawY;
        return true;
    }

    const auto t = ctx->Render.GetMiniMapTransform();
    return ProjectWithMapTransform(t, gx, gy, terrainZ, WorldToGridScale(snap, ctx), sx, sy);
}

inline bool IsFinitePoint(const ImVec2& p) {
    return std::isfinite(p.x) && std::isfinite(p.y);
}

inline float CrossZ(const ImVec2& a, const ImVec2& b, const ImVec2& c) {
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float bcx = c.x - b.x;
    const float bcy = c.y - b.y;
    return abx * bcy - aby * bcx;
}

inline float LengthSq(const ImVec2& a, const ImVec2& b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    return dx * dx + dy * dy;
}

inline bool IsReasonableConvexQuad(const ImVec2 quad[4]) {
    for (int i = 0; i < 4; ++i) {
        if (!IsFinitePoint(quad[i])) return false;
    }

    const float c0 = CrossZ(quad[0], quad[1], quad[2]);
    const float c1 = CrossZ(quad[1], quad[2], quad[3]);
    const float c2 = CrossZ(quad[2], quad[3], quad[0]);
    const float c3 = CrossZ(quad[3], quad[0], quad[1]);
    const bool positive = c0 > 0.f && c1 > 0.f && c2 > 0.f && c3 > 0.f;
    const bool negative = c0 < 0.f && c1 < 0.f && c2 < 0.f && c3 < 0.f;
    if (!positive && !negative) return false;

    const float maxEdgeSq = std::max(
        std::max(LengthSq(quad[0], quad[1]), LengthSq(quad[1], quad[2])),
        std::max(LengthSq(quad[2], quad[3]), LengthSq(quad[3], quad[0])));
    if (maxEdgeSq > 20000.f) return false;

    const float diag0 = LengthSq(quad[0], quad[2]);
    const float diag1 = LengthSq(quad[1], quad[3]);
    if (diag0 > 40000.f || diag1 > 40000.f) return false;

    return true;
}

inline bool BuildTerrainQuad(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                             bool useLargeMap, int gridWidth, int gridHeight, float gx0,
                             float gy0, float gx1, float gy1, ImVec2 out[4]) {
    float sx = 0.f;
    float sy = 0.f;

    if (!ProjectTerrainGridCorner(ctx, snap, useLargeMap, gridWidth, gridHeight, gx0, gy0, sx, sy))
        return false;
    out[0] = ImVec2(sx, sy);

    if (!ProjectTerrainGridCorner(ctx, snap, useLargeMap, gridWidth, gridHeight, gx1, gy0, sx, sy))
        return false;
    out[1] = ImVec2(sx, sy);

    if (!ProjectTerrainGridCorner(ctx, snap, useLargeMap, gridWidth, gridHeight, gx1, gy1, sx, sy))
        return false;
    out[2] = ImVec2(sx, sy);

    if (!ProjectTerrainGridCorner(ctx, snap, useLargeMap, gridWidth, gridHeight, gx0, gy1, sx, sy))
        return false;
    out[3] = ImVec2(sx, sy);
    return true;
}

struct ProjectedTerrainVertex {
    ImVec2 pos{};
    bool   valid = false;
};

inline void BuildProjectedTerrainVertexGrid(std::vector<ProjectedTerrainVertex>& out,
                                            PluginSDK::Context* ctx,
                                            const PluginSDK::Snapshot& snap, bool useLargeMap,
                                            int gridWidth, int gridHeight, int cols, int rows,
                                            float gxMin, float gyMin, float gxSpan, float gySpan) {
    out.clear();
    out.resize(static_cast<size_t>(cols + 1) * static_cast<size_t>(rows + 1));

    for (int row = 0; row <= rows; ++row) {
        const float v = static_cast<float>(row) / static_cast<float>(rows);
        const float gy = gyMin + gySpan * v;

        for (int col = 0; col <= cols; ++col) {
            const float u = static_cast<float>(col) / static_cast<float>(cols);
            const float gx = gxMin + gxSpan * u;
            auto& vertex =
                out[static_cast<size_t>(row) * static_cast<size_t>(cols + 1)
                    + static_cast<size_t>(col)];

            float sx = 0.f;
            float sy = 0.f;
            vertex.valid =
                useLargeMap
                    ? ProjectTerrainGridCorner(ctx, snap, true, gridWidth, gridHeight, gx, gy,
                                               sx, sy)
                    : ProjectTerrainMiniMapCornerSafe(ctx, snap, gridWidth, gridHeight, gx, gy,
                                                      sx, sy);
            if (vertex.valid) vertex.pos = ImVec2(sx, sy);
        }
    }
}

inline void DrawTerrainLargeMap(ImDrawList* dl, PluginSDK::Context* ctx,
                                const PluginSDK::Snapshot& snap, const TerrainTexture& terrain) {
    if (!dl || !terrain.Valid() || terrain.Width() <= 0 || terrain.Height() <= 0) return;

    const int cols = std::clamp(terrain.Width() / 20, 36, 72);
    const int rows = std::clamp(terrain.Height() / 20, 36, 72);
    const float gxMin = -0.25f;
    const float gyMin = -0.25f;
    const float gxSpan = static_cast<float>(terrain.Width());
    const float gySpan = static_cast<float>(terrain.Height());
    std::vector<ProjectedTerrainVertex> vertices;
    BuildProjectedTerrainVertexGrid(vertices, ctx, snap, true, terrain.Width(), terrain.Height(),
                                    cols, rows, gxMin, gyMin, gxSpan, gySpan);

    for (int row = 0; row < rows; ++row) {
        const float v0 = static_cast<float>(row) / static_cast<float>(rows);
        const float v1 = static_cast<float>(row + 1) / static_cast<float>(rows);

        for (int col = 0; col < cols; ++col) {
            const float u0 = static_cast<float>(col) / static_cast<float>(cols);
            const float u1 = static_cast<float>(col + 1) / static_cast<float>(cols);

            const size_t row0 = static_cast<size_t>(row) * static_cast<size_t>(cols + 1);
            const size_t row1 = static_cast<size_t>(row + 1) * static_cast<size_t>(cols + 1);
            const auto& vtx0 = vertices[row0 + static_cast<size_t>(col)];
            const auto& vtx1 = vertices[row0 + static_cast<size_t>(col + 1)];
            const auto& vtx2 = vertices[row1 + static_cast<size_t>(col + 1)];
            const auto& vtx3 = vertices[row1 + static_cast<size_t>(col)];
            if (!vtx0.valid || !vtx1.valid || !vtx2.valid || !vtx3.valid) continue;

            dl->AddImageQuad(terrain.TexRef(), vtx0.pos, vtx1.pos, vtx2.pos, vtx3.pos,
                             ImVec2(u0, v0), ImVec2(u1, v0), ImVec2(u1, v1),
                             ImVec2(u0, v1), IM_COL32_WHITE);
        }
    }
}

inline bool TerrainPatchTouchesMinimap(PluginSDK::Context* ctx, const PluginSDK::MapData& map,
                                       const ImVec2 quad[4]) {
    auto onSurface = [&](const ImVec2& p) {
        return IsOnMinimapSurface(ctx, map, p.x, p.y, 4.f);
    };

    for (const ImVec2& p : {quad[0], quad[1], quad[2], quad[3]}) {
        if (onSurface(p)) return true;
    }

    const ImVec2 center((quad[0].x + quad[1].x + quad[2].x + quad[3].x) * 0.25f,
                        (quad[0].y + quad[1].y + quad[2].y + quad[3].y) * 0.25f);
    if (onSurface(center)) return true;

    const ImVec2 mids[4] = {
        ImVec2((quad[0].x + quad[1].x) * 0.5f, (quad[0].y + quad[1].y) * 0.5f),
        ImVec2((quad[1].x + quad[2].x) * 0.5f, (quad[1].y + quad[2].y) * 0.5f),
        ImVec2((quad[2].x + quad[3].x) * 0.5f, (quad[2].y + quad[3].y) * 0.5f),
        ImVec2((quad[3].x + quad[0].x) * 0.5f, (quad[3].y + quad[0].y) * 0.5f),
    };
    for (const ImVec2& p : mids) {
        if (onSurface(p)) return true;
    }

    return false;
}

inline void DrawTerrainMiniMap(ImDrawList* dl, PluginSDK::Context* ctx,
                               const PluginSDK::Snapshot& snap, const TerrainTexture& terrain) {
    if (!dl || !terrain.Valid() || terrain.Width() <= 0 || terrain.Height() <= 0) return;

    const int cols = std::clamp(terrain.Width() / 12, 48, 96);
    const int rows = std::clamp(terrain.Height() / 12, 48, 96);
    const float gxMin = -0.25f;
    const float gyMin = -0.25f;
    const float gxSpan = static_cast<float>(terrain.Width());
    const float gySpan = static_cast<float>(terrain.Height());
    std::vector<ProjectedTerrainVertex> vertices;
    BuildProjectedTerrainVertexGrid(vertices, ctx, snap, false, terrain.Width(), terrain.Height(),
                                    cols, rows, gxMin, gyMin, gxSpan, gySpan);

    for (int row = 0; row < rows; ++row) {
        const float v0 = static_cast<float>(row) / static_cast<float>(rows);
        const float v1 = static_cast<float>(row + 1) / static_cast<float>(rows);

        for (int col = 0; col < cols; ++col) {
            const float u0 = static_cast<float>(col) / static_cast<float>(cols);
            const float u1 = static_cast<float>(col + 1) / static_cast<float>(cols);
            ImVec2 quad[4];
            const size_t row0 = static_cast<size_t>(row) * static_cast<size_t>(cols + 1);
            const size_t row1 = static_cast<size_t>(row + 1) * static_cast<size_t>(cols + 1);
            const auto& vtx0 = vertices[row0 + static_cast<size_t>(col)];
            const auto& vtx1 = vertices[row0 + static_cast<size_t>(col + 1)];
            const auto& vtx2 = vertices[row1 + static_cast<size_t>(col + 1)];
            const auto& vtx3 = vertices[row1 + static_cast<size_t>(col)];
            if (!vtx0.valid || !vtx1.valid || !vtx2.valid || !vtx3.valid) continue;
            quad[0] = vtx0.pos;
            quad[1] = vtx1.pos;
            quad[2] = vtx2.pos;
            quad[3] = vtx3.pos;

            if (!IsReasonableConvexQuad(quad)) continue;

            const ImVec2 center((quad[0].x + quad[1].x + quad[2].x + quad[3].x) * 0.25f,
                                (quad[0].y + quad[1].y + quad[2].y + quad[3].y) * 0.25f);
            if (!IsOnMinimapSurface(ctx, snap.MiniMap, center.x, center.y, 4.f)) continue;
            if (!TerrainPatchTouchesMinimap(ctx, snap.MiniMap, quad)) continue;

            dl->AddImageQuad(terrain.TexRef(), quad[0], quad[1], quad[2], quad[3],
                             ImVec2(u0, v0), ImVec2(u1, v0), ImVec2(u1, v1),
                             ImVec2(u0, v1), IM_COL32_WHITE);
        }
    }
}

class RadarOverlay {
public:
    RadarData::RadarConfig        cfg;
    RadarData::TargetDatabase     targets;
    RadarData::IconTables         icons;
    IconAtlas                     atlas;
    TerrainTexture                terrain;
    RadarPerf::AreaCacheState     cache;
    PluginSDK::WalkableGridHandle walkable;
    bool                          mapWasVisible = false;

    bool ShouldDraw(const PluginSDK::Snapshot& snap) const {
        if (!cfg.OverlayEnabled) return false;
        if (!snap.IsAttached) return false;
        if (snap.State != PluginSDK::GameState::InGame) return false;
        if (cfg.DrawWhenNotInHideoutOrTown && (snap.IsTown || snap.IsHideout)) return false;
        if (cfg.DrawWhenNotPaused && snap.IsPaused) return false;
        if (cfg.HideWhenNotForeground && !snap.GameWindowForeground) return false;
        return true;
    }

    void EnsureAtlas(PluginSDK::Context* ctx, const std::filesystem::path& pluginDir) {
        if (atlas.Valid()) return;
        const auto path = pluginDir / "assets" / "icons.png";
        if (!atlas.Load(ctx->D3DDevice, path, icons.maxCx, icons.maxCy)) {
            RadarData::RadarLog::Instance().Error("Failed to load icon atlas: " + path.string());
        } else {
            RadarData::RadarLog::Instance().Info("Icon atlas loaded: " + path.string());
        }
    }

    void Draw(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap) {
        if (!ShouldDraw(snap)) return;

        auto current = ctx->Terrain.GetWalkableGrid();
        if (current.Data() != walkable.Data()) walkable = std::move(current);

        const bool mapVisible = snap.LargeMap.IsVisible || snap.MiniMap.IsVisible;
        if (mapVisible && !mapWasVisible) cache.InvalidatePoi();
        mapWasVisible = mapVisible;

        if (cache.NeedsFullRebuild(snap, walkable.Data())) {
            cache.RebuildAll(ctx, snap, walkable, cfg, targets, icons);
        } else {
            cache.PollPoiDiscovery(ctx, snap, cfg, targets);
            cache.RebuildPoiIfNeeded(ctx, snap, cfg, targets, icons);
            if (cache.NeedsEntityRebuild(snap))
                cache.RebuildEntitiesOnly(ctx, snap, cfg, targets, icons);
        }

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (!dl) {
            RadarData::RadarLog::Instance().Warn("DrawUI: no background draw list");
            return;
        }

        const bool terrainReady =
            cfg.DrawWalkableMap
            && terrain.EnsureBuilt(ctx->D3DDevice, cache.walkable, cfg, snap.AreaChangeCounter,
                                   walkable.Data());

        if (snap.LargeMap.IsVisible) {
            MapClipScope clip(dl, snap.LargeMap, false);
            if (terrainReady) DrawTerrainLargeMap(dl, ctx, snap, terrain);
            cache.entities.Draw(ctx, snap, dl);
            if (cfg.ShowImportantPOI) {
                cache.pois.UpdateScreenPositions(ctx, snap);
                cache.pois.Draw(dl, atlas, cfg, cfg.EdgeIndicatorLargemap, cfg.EdgeIndicatorMinimap,
                                ctx, &snap);
                cache.pois.DrawEdgeIndicators(ctx, dl, snap.LargeMap, cfg.EdgeIndicatorLargemap,
                                              false);
            } else {
                cache.pois.Clear();
            }
        } else if (snap.MiniMap.IsVisible) {
            MapClipScope clip(dl, snap.MiniMap, true);
            if (terrainReady && cfg.DrawMiniMapTerrain) DrawTerrainMiniMap(dl, ctx, snap, terrain);
            if (cfg.DrawMiniMapEntities) cache.entities.Draw(ctx, snap, dl);
            if (cfg.ShowImportantPOI) {
                cache.pois.UpdateScreenPositions(ctx, snap);
                cache.pois.Draw(dl, atlas, cfg, cfg.EdgeIndicatorLargemap, cfg.EdgeIndicatorMinimap,
                                ctx, &snap);
                cache.pois.DrawEdgeIndicators(ctx, dl, snap.MiniMap, cfg.EdgeIndicatorMinimap, true);
            } else {
                cache.pois.Clear();
            }
        }
    }
};

} // namespace RadarRender
