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

struct TerrainGridExtents {
    float gxMin = -0.25f;
    float gyMin = -0.25f;
    float gxSpan = 0.f;
    float gySpan = 0.f;
};

struct TerrainAnchorDelta {
    ImVec2 delta{};
    bool   valid = false;
};

inline TerrainGridExtents ResolveTerrainGridExtents(const RadarData::RadarConfig& cfg,
                                                    const TerrainTexture& terrain) {
    (void)cfg;
    TerrainGridExtents extents;
    extents.gxMin = -0.5f;
    extents.gyMin = -0.5f;
    extents.gxSpan = static_cast<float>(terrain.Width());
    extents.gySpan = static_cast<float>(terrain.Height());
    return extents;
}

inline bool UsesTextureTerrain(const RadarData::RadarConfig& cfg) {
    return cfg.TerrainStyle == RadarData::TerrainRenderStyle::Texture
           || cfg.TerrainStyle == RadarData::TerrainRenderStyle::TextureAndDotMatrix;
}

inline bool UsesDotMatrixTerrain(const RadarData::RadarConfig& cfg) {
    return cfg.TerrainStyle == RadarData::TerrainRenderStyle::DotMatrix
           || cfg.TerrainStyle == RadarData::TerrainRenderStyle::TextureAndDotMatrix;
}

inline bool UsesTerrainBoundaryLines(const RadarData::RadarConfig& cfg) {
    return UsesTextureTerrain(cfg) && cfg.WalkableMapBorderThickness > 0;
}

inline float ClampTerrainGrid(float value, int extent) {
    if (extent <= 0) return 0.f;
    const float maxValue = static_cast<float>(extent - 1);
    return std::clamp(value, 0.f, maxValue);
}

inline float ResolveTerrainProjectionZ(PluginSDK::Context* ctx,
                                       const RadarData::RadarConfig& cfg,
                                       int gridWidth, int gridHeight, float gx, float gy) {
    if (cfg.TerrainHeightMode == RadarData::TerrainProjectionHeightMode::Flat
        || cfg.TerrainHeightMode == RadarData::TerrainProjectionHeightMode::FlatPlayerAnchored)
        return 0.f;
    const float sampleX = ClampTerrainGrid(gx, gridWidth);
    const float sampleY = ClampTerrainGrid(gy, gridHeight);
    return TerrainHeightAtGrid(ctx, sampleX, sampleY);
}

inline bool ProjectTerrainWithSdk(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                                  bool useLargeMap, float gx, float gy, float terrainZ,
                                  float& sx, float& sy) {
    if (!ctx) return false;
    if (useLargeMap) return ctx->Render.GridToLargeMap(gx, gy, terrainZ, sx, sy);
    float rawX = 0.f;
    float rawY = 0.f;
    if (!ctx->Render.GridToMiniMap(gx, gy, terrainZ, rawX, rawY)) return false;
    if (MiniMapGridLooksLikeViewport(ctx, snap, rawX, rawY)) return false;
    sx = rawX;
    sy = rawY;
    return true;
}

inline void ApplyTerrainAnchorDelta(const TerrainAnchorDelta* anchor, float& sx, float& sy) {
    if (!anchor || !anchor->valid) return;
    sx += anchor->delta.x;
    sy += anchor->delta.y;
}

inline ProjectedScreen ProjectPlayerMarkerScreen(PluginSDK::Context* ctx,
                                                 const PluginSDK::Snapshot& snap,
                                                 bool useLargeMap) {
    if (!snap.Player.IsValid) return {};
    if (useLargeMap)
        return ProjectGridToLargeMapScreen(ctx, snap, snap.Player.GridPositionX,
                                           snap.Player.GridPositionY, snap.Player.TerrainHeight);
    return ProjectEntityToMiniMapScreen(ctx, snap, snap.Player.GridPositionX,
                                        snap.Player.GridPositionY, snap.Player.TerrainHeight);
}

inline TerrainAnchorDelta ComputeTerrainPlayerAnchorDelta(PluginSDK::Context* ctx,
                                                          const PluginSDK::Snapshot& snap,
                                                          const RadarData::RadarConfig& cfg,
                                                          bool useLargeMap, int gridWidth,
                                                          int gridHeight) {
    TerrainAnchorDelta out;
    if (!ctx || !snap.Player.IsValid
        || cfg.TerrainHeightMode != RadarData::TerrainProjectionHeightMode::FlatPlayerAnchored)
        return out;

    const auto normalPlayer = ProjectPlayerMarkerScreen(ctx, snap, useLargeMap);
    if (!normalPlayer.valid) return out;

    float flatSx = 0.f;
    float flatSy = 0.f;
    const bool flatProjected =
        ProjectTerrainWithSdk(ctx, snap, useLargeMap, snap.Player.GridPositionX,
                              snap.Player.GridPositionY,
                              ResolveTerrainProjectionZ(ctx, cfg, gridWidth, gridHeight,
                                                        snap.Player.GridPositionX,
                                                        snap.Player.GridPositionY),
                              flatSx, flatSy);
    if (!flatProjected) return out;

    out.delta = ImVec2(normalPlayer.sx - flatSx, normalPlayer.sy - flatSy);
    out.valid = true;
    return out;
}

inline bool ProjectTerrainGridCorner(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                                     const RadarData::RadarConfig& cfg, bool useLargeMap,
                                     int gridWidth, int gridHeight, float gx, float gy, float& sx,
                                     float& sy, const TerrainAnchorDelta* anchor = nullptr,
                                     const MapLayerProjection* largeMapProj = nullptr) {
    if (!ctx) return false;
    const float terrainZ = ResolveTerrainProjectionZ(ctx, cfg, gridWidth, gridHeight, gx, gy);
    if (useLargeMap && largeMapProj) {
        const auto projected =
            ProjectGridLargeMapLayer(*largeMapProj, ctx, snap, gx, gy, terrainZ,
                                     MapLayerSubject::Terrain);
        if (!projected.valid) return false;
        sx = projected.sx;
        sy = projected.sy;
    } else {
        if (!ProjectTerrainWithSdk(ctx, snap, useLargeMap, gx, gy, terrainZ, sx, sy)) return false;
    }
    if (!useLargeMap || !largeMapProj
        || largeMapProj->mode == RadarData::MapLayerProjectionMode::NativeSdk)
        ApplyTerrainAnchorDelta(anchor, sx, sy);
    return true;
}

inline bool ProjectTerrainMiniMapCornerSafe(PluginSDK::Context* ctx,
                                            const PluginSDK::Snapshot& snap,
                                            const RadarData::RadarConfig& cfg, int gridWidth,
                                            int gridHeight, float gx, float gy, float& sx, float& sy,
                                            const TerrainAnchorDelta* anchor = nullptr) {
    return ProjectTerrainGridCorner(ctx, snap, cfg, false, gridWidth, gridHeight, gx, gy, sx, sy,
                                    anchor);
}

inline float TerrainBoundaryGridX(const TerrainGridExtents& extents, int cellX) {
    return extents.gxMin + static_cast<float>(cellX);
}

inline float TerrainBoundaryGridY(const TerrainGridExtents& extents, int cellY) {
    return extents.gyMin + static_cast<float>(cellY);
}

struct ProjectedTerrainVertex {
    ImVec2 pos{};
    bool   valid = false;
};

inline void BuildProjectedTerrainVertexGrid(std::vector<ProjectedTerrainVertex>& out,
                                            PluginSDK::Context* ctx,
                                            const PluginSDK::Snapshot& snap,
                                            const RadarData::RadarConfig& cfg,
                                            int gridWidth, int gridHeight, int cols, int rows,
                                            float gxMin, float gyMin, float gxSpan, float gySpan,
                                            const TerrainAnchorDelta* anchor = nullptr,
                                            const MapLayerProjection* largeMapProj = nullptr) {
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
            vertex.valid = ProjectTerrainGridCorner(ctx, snap, cfg, true, gridWidth, gridHeight,
                                                    gx, gy, sx, sy, anchor, largeMapProj);
            if (vertex.valid) vertex.pos = ImVec2(sx, sy);
        }
    }
}

inline void DrawTerrainLargeMap(ImDrawList* dl, PluginSDK::Context* ctx,
                                const PluginSDK::Snapshot& snap, const TerrainTexture& terrain,
                                const RadarData::RadarConfig& cfg,
                                const MapLayerProjection& largeMapProj) {
    if (!dl || !terrain.Valid() || terrain.Width() <= 0 || terrain.Height() <= 0) return;

    const int cols = std::clamp(terrain.Width() / 20, 36, 72);
    const int rows = std::clamp(terrain.Height() / 20, 36, 72);
    const TerrainGridExtents extents = ResolveTerrainGridExtents(cfg, terrain);
    std::vector<ProjectedTerrainVertex> vertices;
    BuildProjectedTerrainVertexGrid(vertices, ctx, snap, cfg, terrain.Width(),
                                    terrain.Height(), cols, rows, extents.gxMin, extents.gyMin,
                                    extents.gxSpan, extents.gySpan, nullptr,
                                    &largeMapProj);

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

inline void DrawTerrainDotMatrix(ImDrawList* dl, PluginSDK::Context* ctx,
                                 const PluginSDK::Snapshot& snap,
                                 const WalkableBake& walkable,
                                 const RadarData::RadarConfig& cfg, bool useLargeMap,
                                 const MapLayerProjection* largeMapProj = nullptr) {
    if (!dl || !ctx || !walkable.valid || walkable.width <= 0 || walkable.height <= 0
        || walkable.walkableMask.empty())
        return;

    const ImU32 fillColor =
        ImGui::ColorConvertFloat4ToU32(cfg.DotMatrixFillColor);

    const int step = std::max(1, cfg.DotCellStep);
    const float halfSize = std::clamp(cfg.DotSize, 0.5f, 6.0f);
    const bool useAnchor = !useLargeMap;
    const TerrainAnchorDelta anchor =
        useAnchor ? ComputeTerrainPlayerAnchorDelta(ctx, snap, cfg, useLargeMap, walkable.width,
                                                    walkable.height)
                  : TerrainAnchorDelta{};

    for (int gy = 0; gy < walkable.height; gy += step) {
        for (int gx = 0; gx < walkable.width; gx += step) {
            const size_t idx = static_cast<size_t>(gy) * static_cast<size_t>(walkable.width)
                             + static_cast<size_t>(gx);
            if (idx >= walkable.walkableMask.size() || walkable.walkableMask[idx] == 0) continue;

            float sx = 0.f;
            float sy = 0.f;
            const bool projected =
                useLargeMap
                    ? ProjectTerrainGridCorner(ctx, snap, cfg, true, walkable.width,
                                               walkable.height, static_cast<float>(gx),
                                               static_cast<float>(gy), sx, sy,
                                               useAnchor ? &anchor : nullptr, largeMapProj)
                    : ProjectTerrainMiniMapCornerSafe(ctx, snap, cfg, walkable.width,
                                                      walkable.height, static_cast<float>(gx),
                                                      static_cast<float>(gy), sx, sy,
                                                      useAnchor ? &anchor : nullptr);
            if (!projected) continue;

            if (useLargeMap) {
                if (!IsInsideMapRect(snap.LargeMap, sx, sy)) continue;
            } else if (!IsOnMinimapSurface(ctx, snap.MiniMap, sx, sy, 10.f)) {
                continue;
            }

            dl->AddRectFilled(ImVec2(sx - halfSize, sy - halfSize),
                              ImVec2(sx + halfSize, sy + halfSize), fillColor);
        }
    }
}

inline void DrawTerrainBoundaryLines(ImDrawList* dl, PluginSDK::Context* ctx,
                                     const PluginSDK::Snapshot& snap,
                                     const WalkableBake& walkable,
                                     const RadarData::RadarConfig& cfg, bool useLargeMap,
                                     const MapLayerProjection* largeMapProj = nullptr) {
    if (!dl || !ctx || !walkable.valid || walkable.boundarySegments.empty()) return;

    TerrainTexture terrainView;
    const TerrainGridExtents extents = ResolveTerrainGridExtents(cfg, terrainView);
    const ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(cfg.TextureWallEdgeColor);
    const float thickness = std::clamp(static_cast<float>(cfg.WalkableMapBorderThickness), 1.0f, 8.0f);
    const bool useAnchor = !useLargeMap;
    const TerrainAnchorDelta anchor =
        useAnchor ? ComputeTerrainPlayerAnchorDelta(ctx, snap, cfg, useLargeMap, walkable.width,
                                                    walkable.height)
                  : TerrainAnchorDelta{};

    for (const auto& segment : walkable.boundarySegments) {
        const float gx0 = TerrainBoundaryGridX(extents, segment.x0);
        const float gy0 = TerrainBoundaryGridY(extents, segment.y0);
        const float gx1 = TerrainBoundaryGridX(extents, segment.x1);
        const float gy1 = TerrainBoundaryGridY(extents, segment.y1);

        float sx0 = 0.f, sy0 = 0.f;
        float sx1 = 0.f, sy1 = 0.f;
        const bool ok0 =
            useLargeMap ? ProjectTerrainGridCorner(ctx, snap, cfg, true, walkable.width,
                                                   walkable.height, gx0, gy0, sx0, sy0,
                                                   useAnchor ? &anchor : nullptr, largeMapProj)
                        : ProjectTerrainMiniMapCornerSafe(ctx, snap, cfg, walkable.width,
                                                          walkable.height, gx0, gy0, sx0, sy0,
                                                          useAnchor ? &anchor : nullptr);
        const bool ok1 =
            useLargeMap ? ProjectTerrainGridCorner(ctx, snap, cfg, true, walkable.width,
                                                   walkable.height, gx1, gy1, sx1, sy1,
                                                   useAnchor ? &anchor : nullptr, largeMapProj)
                        : ProjectTerrainMiniMapCornerSafe(ctx, snap, cfg, walkable.width,
                                                          walkable.height, gx1, gy1, sx1, sy1,
                                                          useAnchor ? &anchor : nullptr);
        if (!ok0 || !ok1) continue;

        if (useLargeMap) {
            dl->AddLine(ImVec2(sx0, sy0), ImVec2(sx1, sy1), lineColor, thickness);
            continue;
        }

        const ImVec2 p0(sx0, sy0);
        const ImVec2 p1(sx1, sy1);
        const ImVec2 mid((sx0 + sx1) * 0.5f, (sy0 + sy1) * 0.5f);
        if (!IsOnMinimapSurface(ctx, snap.MiniMap, p0.x, p0.y, 4.f)
            && !IsOnMinimapSurface(ctx, snap.MiniMap, p1.x, p1.y, 4.f)
            && !IsOnMinimapSurface(ctx, snap.MiniMap, mid.x, mid.y, 4.f))
            continue;

        dl->AddLine(p0, p1, lineColor, thickness);
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

        const bool mapVisible = snap.LargeMap.IsVisible || snap.MiniMap.IsVisible;
        if (!mapVisible) {
            mapWasVisible = false;
            return;
        }
        if (!mapWasVisible) cache.InvalidatePoi();
        mapWasVisible = true;

        auto current = ctx->Terrain.GetWalkableGrid();
        if (current.Data() != walkable.Data()) walkable = std::move(current);

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

        if (snap.LargeMap.IsVisible) {
            const bool terrainReady =
                cfg.DrawWalkableMap && UsesTextureTerrain(cfg)
                && terrain.EnsureBuilt(ctx->D3DDevice, cache.walkable, cfg,
                                       snap.AreaChangeCounter, walkable.Data());
            const MapLayerProjection largeMapProj = BuildLargeMapLayerProjection(ctx, snap, cfg);
            MapClipScope clip(dl, snap.LargeMap, false);
            if (terrainReady && largeMapProj.valid)
                DrawTerrainLargeMap(dl, ctx, snap, terrain, cfg, largeMapProj);
            if (cfg.DrawWalkableMap && UsesDotMatrixTerrain(cfg))
                DrawTerrainDotMatrix(dl, ctx, snap, cache.walkable, cfg, true, &largeMapProj);
            if (cfg.DrawWalkableMap && UsesTerrainBoundaryLines(cfg))
                DrawTerrainBoundaryLines(dl, ctx, snap, cache.walkable, cfg, true,
                                         &largeMapProj);
            cache.entities.Draw(ctx, snap, dl, &largeMapProj);
            if (cfg.ShowImportantPOI) {
                cache.pois.UpdateScreenPositions(ctx, snap, &largeMapProj);
                cache.pois.Draw(dl, atlas, cfg, cfg.EdgeIndicatorLargemap, cfg.EdgeIndicatorMinimap,
                                ctx, &snap);
                cache.pois.DrawEdgeIndicators(ctx, dl, snap.LargeMap, cfg.EdgeIndicatorLargemap,
                                              false);
            } else {
                cache.pois.Clear();
            }
        } else if (snap.MiniMap.IsVisible) {
            MapClipScope clip(dl, snap.MiniMap, true);
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
