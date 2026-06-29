#pragma once

#include "data/RadarConfig.h"
#include "sdk/PluginSDK.h"

#include <imgui.h>

#include <cmath>

namespace RadarRender {

enum class MapLayerSubject {
    Terrain,
    Entity,
    Poi,
    TargetPoi,
};

struct ProjectedScreen {
    float sx = 0.f;
    float sy = 0.f;
    bool  valid = false;
};

struct MapLayerProjection {
    RadarData::MapLayerProjectionMode mode = RadarData::MapLayerProjectionMode::Unified2D;
    bool                              valid = false;
    PluginSDK::MapData                map{};
    PluginSDK::MapTransform           transform{};
};

inline ProjectedScreen ProjectTgtToLargeMapScreen(PluginSDK::Context* ctx,
                                                  const PluginSDK::Snapshot& snap,
                                                  float gx, float gy);
inline ProjectedScreen ProjectEntityGridToScreen(PluginSDK::Context* ctx,
                                                 const PluginSDK::Snapshot& snap, float gx,
                                                 float gy, float terrainZ);

inline float WorldToGridScale(const PluginSDK::Snapshot& snap, PluginSDK::Context* ctx) {
    if (snap.WorldToGridConvertor > 1.f) return snap.WorldToGridConvertor;
    if (ctx) {
        const float w = ctx->Terrain.GetWorldToGridConvertor();
        if (w > 1.f) return w;
    }
    return 10.25f;
}

inline bool ProjectWithMapTransform(const PluginSDK::MapTransform& t, float gx, float gy,
                                    float worldZ, float worldToGrid, float& sx, float& sy) {
    if (!t.IsVisible) return false;
    const float dx = gx - t.PlayerGridX;
    const float dy = gy - t.PlayerGridY;
    sx = t.CenterX + (dx - dy) * t.ScaleX;
    sy = t.CenterY + (worldZ * worldToGrid - (dx + dy)) * t.ScaleY;
    return true;
}

inline bool IsInsideMapRect(const PluginSDK::MapData& map, float sx, float sy,
                            float margin = 2.f) {
    if (!map.IsVisible || map.SizeX <= 0.f || map.SizeY <= 0.f) return false;
    const float halfW = map.SizeX * 0.5f;
    const float halfH = map.SizeY * 0.5f;
    return sx >= map.CenterX - halfW + margin && sx <= map.CenterX + halfW - margin
           && sy >= map.CenterY - halfH + margin && sy <= map.CenterY + halfH - margin;
}

inline float MinimapClipRadius(const PluginSDK::MapData& map, float inset = 10.f) {
    if (!map.IsVisible) return 0.f;
    return std::max(1.f, std::min(map.SizeX, map.SizeY) * 0.5f - inset);
}

// GridToMiniMap projects around GetMiniMapTransform().Center, not MapData.Center (host Y often wrong).
inline void GetMinimapClipOrigin(PluginSDK::Context* ctx, const PluginSDK::MapData& map,
                                 float& cx, float& cy) {
    cx = map.CenterX;
    cy = map.CenterY;
    if (!ctx) return;
    const auto t = ctx->Render.GetMiniMapTransform();
    if (t.IsVisible) {
        cx = t.CenterX;
        cy = t.CenterY;
    }
}

inline float MinimapSurfaceRadius(const PluginSDK::MapData& map, float inset = 6.f) {
    if (!map.IsVisible) return 0.f;
    return std::max(20.f, std::min(map.SizeX, map.SizeY) * 0.5f - inset);
}

inline bool IsOnMinimapSurface(PluginSDK::Context* ctx, const PluginSDK::MapData& map,
                               float sx, float sy, float inset = 6.f) {
    if (!map.IsVisible) return false;
    float cx = 0.f, cy = 0.f;
    GetMinimapClipOrigin(ctx, map, cx, cy);
    const float r = MinimapSurfaceRadius(map, inset);
    const float dx = sx - cx;
    const float dy = sy - cy;
    return (dx * dx + dy * dy) <= (r * r);
}

inline bool IsInsideMinimapDisc(PluginSDK::Context* ctx, const PluginSDK::MapData& map,
                                float sx, float sy, float inset = 10.f) {
    return IsOnMinimapSurface(ctx, map, sx, sy, inset);
}

inline bool MiniMapGridLooksLikeViewport(PluginSDK::Context* ctx,
                                         const PluginSDK::Snapshot& snap, float sx, float sy) {
    if (IsOnMinimapSurface(ctx, snap.MiniMap, sx, sy)) return false;
    if (snap.ScreenWidth <= 1 || snap.ScreenHeight <= 1) return false;
    constexpr float margin = 12.f;
    return sx >= margin && sx < snap.ScreenWidth - margin && sy >= margin
           && sy < snap.ScreenHeight - margin;
}

inline bool IsInsideMapViewport(PluginSDK::Context* ctx, const PluginSDK::MapData& map,
                                float sx, float sy, bool minimap) {
    return minimap ? IsInsideMinimapDisc(ctx, map, sx, sy) : IsInsideMapRect(map, sx, sy);
}

inline void PushLargeMapClipRect(ImDrawList* dl, const PluginSDK::MapData& map) {
    if (!dl || !map.IsVisible) return;
    const float halfW = map.SizeX * 0.5f;
    const float halfH = map.SizeY * 0.5f;
    dl->PushClipRect(ImVec2(map.CenterX - halfW, map.CenterY - halfH),
                     ImVec2(map.CenterX + halfW, map.CenterY + halfH), true);
}

struct MapClipScope {
    ImDrawList*                 dl = nullptr;
    bool                        pushed = false;

    // Minimap is circular in-game; use per-primitive disc tests only (no square clip rect).
    MapClipScope(ImDrawList* drawList, const PluginSDK::MapData& map, bool minimap) : dl(drawList) {
        if (dl && map.IsVisible && !minimap) {
            PushLargeMapClipRect(dl, map);
            pushed = true;
        }
    }

    ~MapClipScope() {
        if (pushed && dl) dl->PopClipRect();
    }

    MapClipScope(const MapClipScope&) = delete;
    MapClipScope& operator=(const MapClipScope&) = delete;
};

inline ProjectedScreen ProjectGridToLargeMapScreen(PluginSDK::Context* ctx,
                                                   const PluginSDK::Snapshot& snap,
                                                   float gx, float gy, float worldZ) {
    ProjectedScreen out;
    if (!ctx || !snap.LargeMap.IsVisible) return out;

    if (ctx->Render.GridToLargeMap(gx, gy, worldZ, out.sx, out.sy)
        && IsInsideMapRect(snap.LargeMap, out.sx, out.sy)) {
        out.valid = true;
        return out;
    }

    const auto t = ctx->Render.GetLargeMapTransform();
    const float wtg = WorldToGridScale(snap, ctx);
    float sx = 0.f, sy = 0.f;
    if (ProjectWithMapTransform(t, gx, gy, worldZ, wtg, sx, sy)
        && IsInsideMapRect(snap.LargeMap, sx, sy)) {
        out.sx = sx;
        out.sy = sy;
        out.valid = true;
    }
    return out;
}

inline MapLayerProjection BuildLargeMapLayerProjection(PluginSDK::Context* ctx,
                                                       const PluginSDK::Snapshot& snap,
                                                       const RadarData::RadarConfig& cfg) {
    MapLayerProjection out;
    (void)cfg;
    out.mode = RadarData::MapLayerProjectionMode::Unified2D;
    out.map = snap.LargeMap;
    if (!snap.LargeMap.IsVisible) return out;

    if (!ctx) return out;

    out.transform = ctx->Render.GetLargeMapTransform();
    out.valid = out.transform.IsVisible;
    return out;
}

inline ProjectedScreen ProjectGridUnified2D(const MapLayerProjection& proj, float gx, float gy) {
    ProjectedScreen out;
    if (!proj.valid || proj.mode != RadarData::MapLayerProjectionMode::Unified2D
        || !proj.map.IsVisible || !proj.transform.IsVisible)
        return out;

    const float dx = gx - proj.transform.PlayerGridX;
    const float dy = gy - proj.transform.PlayerGridY;
    out.sx = proj.transform.CenterX + (dx - dy) * proj.transform.ScaleX;
    out.sy = proj.transform.CenterY - (dx + dy) * proj.transform.ScaleY;
    out.valid = IsInsideMapRect(proj.map, out.sx, out.sy);
    return out;
}

inline ProjectedScreen ProjectGridLargeMapLayer(const MapLayerProjection& proj,
                                                PluginSDK::Context* ctx,
                                                const PluginSDK::Snapshot& snap, float gx,
                                                float gy, float terrainZ,
                                                MapLayerSubject subject) {
    (void)ctx;
    (void)snap;
    (void)terrainZ;
    (void)subject;
    if (!proj.valid) return {};
    return ProjectGridUnified2D(proj, gx, gy);
}

inline ProjectedScreen ProjectGridToMiniMapScreen(PluginSDK::Context* ctx,
                                                  const PluginSDK::Snapshot& snap,
                                                  float gx, float gy, float worldZ,
                                                  float discInset = 10.f,
                                                  bool allowTransformFallback = true) {
    ProjectedScreen out;
    if (!ctx || !snap.MiniMap.IsVisible) return out;

    if (ctx->Render.GridToMiniMap(gx, gy, worldZ, out.sx, out.sy)
        && IsOnMinimapSurface(ctx, snap.MiniMap, out.sx, out.sy, discInset)) {
        out.valid = true;
        return out;
    }

    if (!allowTransformFallback) return out;

    const auto t = ctx->Render.GetMiniMapTransform();
    const float wtg = WorldToGridScale(snap, ctx);
    float sx = 0.f, sy = 0.f;
    if (ProjectWithMapTransform(t, gx, gy, worldZ, wtg, sx, sy)
        && IsOnMinimapSurface(ctx, snap.MiniMap, sx, sy, discInset)) {
        out.sx = sx;
        out.sy = sy;
        out.valid = true;
    }
    return out;
}

inline float TerrainHeightAtGrid(PluginSDK::Context* ctx, float gx, float gy) {
    if (!ctx) return 0.f;
    return ctx->Terrain.GetTerrainHeight(static_cast<int>(gx), static_cast<int>(gy));
}

inline ProjectedScreen ProjectGridToMapScreen(PluginSDK::Context* ctx,
                                              const PluginSDK::Snapshot& snap,
                                              float gx, float gy, float terrainZ) {
    if (snap.LargeMap.IsVisible) {
        const auto out = ProjectGridToLargeMapScreen(ctx, snap, gx, gy, terrainZ);
        if (out.valid) return out;
    }
    return ProjectGridToMiniMapScreen(ctx, snap, gx, gy, terrainZ);
}

inline ProjectedScreen ProjectTgtToLargeMapScreen(PluginSDK::Context* ctx,
                                                  const PluginSDK::Snapshot& snap,
                                                  float gx, float gy) {
    return ProjectGridToLargeMapScreen(ctx, snap, gx, gy, TerrainHeightAtGrid(ctx, gx, gy));
}

inline ProjectedScreen ProjectTgtToMiniMapScreen(PluginSDK::Context* ctx,
                                                 const PluginSDK::Snapshot& snap,
                                                 float gx, float gy) {
    return ProjectGridToMiniMapScreen(ctx, snap, gx, gy, TerrainHeightAtGrid(ctx, gx, gy));
}

inline ProjectedScreen ProjectTgtToMapScreen(PluginSDK::Context* ctx,
                                             const PluginSDK::Snapshot& snap,
                                             float gx, float gy) {
    return ProjectGridToMapScreen(ctx, snap, gx, gy, TerrainHeightAtGrid(ctx, gx, gy));
}

inline ProjectedScreen ProjectGridToScreen(PluginSDK::Context* ctx,
                                           const PluginSDK::Snapshot& snap,
                                           float gx, float gy, float terrainZ) {
    ProjectedScreen out;
    if (!ctx) return out;

    if (snap.LargeMap.IsVisible || snap.MiniMap.IsVisible)
        return ProjectGridToMapScreen(ctx, snap, gx, gy, terrainZ);

    if (snap.Player.IsValid) {
        const float conv = WorldToGridScale(snap, ctx);
        const float wx =
            snap.Player.WorldX + (gx - snap.Player.GridPositionX) * conv;
        const float wy =
            snap.Player.WorldY + (gy - snap.Player.GridPositionY) * conv;
        if (ctx->Render.WorldToScreen(wx, wy, terrainZ, out.sx, out.sy)) {
            out.valid = true;
            return out;
        }
    }

    return out;
}

// Entities on minimap: GridToMiniMap is correct for off-screen targets; on-screen targets can
// return main-viewport coords. Use MapTransform only in that case. Always require on-surface clip.
inline ProjectedScreen ProjectEntityToMiniMapScreen(PluginSDK::Context* ctx,
                                                    const PluginSDK::Snapshot& snap,
                                                    float gx, float gy, float terrainZ) {
    ProjectedScreen out;
    if (!ctx || !snap.MiniMap.IsVisible) return out;

    constexpr float kInset = 8.f;
    float sx = 0.f, sy = 0.f;
    const bool gridOk = ctx->Render.GridToMiniMap(gx, gy, terrainZ, sx, sy);
    if (gridOk && IsOnMinimapSurface(ctx, snap.MiniMap, sx, sy, kInset)) {
        out.sx = sx;
        out.sy = sy;
        out.valid = true;
        return out;
    }

    const bool needTransform = !gridOk || MiniMapGridLooksLikeViewport(ctx, snap, sx, sy);
    if (needTransform) {
        const float wtg = WorldToGridScale(snap, ctx);
        const auto t = ctx->Render.GetMiniMapTransform();
        float tsx = 0.f, tsy = 0.f;
        if (t.IsVisible && ProjectWithMapTransform(t, gx, gy, terrainZ, wtg, tsx, tsy)
            && IsOnMinimapSurface(ctx, snap.MiniMap, tsx, tsy, kInset)) {
            out.sx = tsx;
            out.sy = tsy;
            out.valid = true;
        }
    }
    return out;
}

inline ProjectedScreen ProjectEntityGridToScreen(PluginSDK::Context* ctx,
                                                 const PluginSDK::Snapshot& snap,
                                                 float gx, float gy, float terrainZ) {
    if (snap.LargeMap.IsVisible)
        return ProjectGridToLargeMapScreen(ctx, snap, gx, gy, terrainZ);
    if (snap.MiniMap.IsVisible)
        return ProjectEntityToMiniMapScreen(ctx, snap, gx, gy, terrainZ);
    return {};
}

inline ProjectedScreen ProjectEntityToScreen(PluginSDK::Context* ctx,
                                             const PluginSDK::Snapshot& snap,
                                             const PluginSDK::Entity& e) {
    ProjectedScreen out;
    if (!ctx || !e.IsValid) return out;

    if (snap.LargeMap.IsVisible || snap.MiniMap.IsVisible)
        return ProjectEntityGridToScreen(ctx, snap, e.GridPositionX, e.GridPositionY,
                                         e.TerrainHeight);

    if (ctx->Render.WorldToScreen(e.WorldX, e.WorldY, e.WorldZ, out.sx, out.sy))
        out.valid = true;
    return out;
}

} // namespace RadarRender
