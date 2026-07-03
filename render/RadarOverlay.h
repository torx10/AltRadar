#pragma once

#include "MapProjection.h"
#include "TerrainTexture.h"
#include "perf/AreaCache.h"
#include "perf/PerfTiming.h"
#include "data/RadarConfig.h"
#include "data/RadarLog.h"
#include "data/TargetDatabase.h"
#include "data/IconTables.h"
#include "ui/UiBlockerDetector.h"
#include "sdk/PluginSDK.h"

#include <algorithm>
#include <filesystem>
#include <imgui.h>
#include <string>
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

struct TerrainDetailDrawStats {
    int  total = 0;
    int  eligible = 0;
    int  drawn = 0;
    int  culled = 0;
    int  culledProjectionFailed = 0;
    int  culledOutsideSurface = 0;
    int  culledBudget = 0;
    bool budgetHit = false;
    bool timeBudgetHit = false;
    int  effectiveStep = 1;
    int  stride = 1;
    int  targetDrawCount = 0;
    double timeBudgetMs = 0.0;
    bool adaptiveSamplingActive = false;
};

struct TerrainTextureDrawStats {
    bool  submitted = false;
    const char* skippedReason = nullptr;
    float tintAlpha = 0.0f;
    bool  rectValid = false;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
};

inline int ResolveBoundaryTargetDrawCount(RadarData::TerrainBoundaryQualityMode mode) {
    switch (mode) {
        case RadarData::TerrainBoundaryQualityMode::Off: return 0;
        case RadarData::TerrainBoundaryQualityMode::Low: return 1500;
        case RadarData::TerrainBoundaryQualityMode::Medium: return 3000;
        case RadarData::TerrainBoundaryQualityMode::High: return 6000;
        case RadarData::TerrainBoundaryQualityMode::Unlimited: return 0;
        default: return 3000;
    }
}

inline double ResolveBoundaryTimeBudgetMs(RadarData::TerrainBoundaryQualityMode mode) {
    switch (mode) {
        case RadarData::TerrainBoundaryQualityMode::Low: return 2.0;
        case RadarData::TerrainBoundaryQualityMode::Medium: return 4.0;
        case RadarData::TerrainBoundaryQualityMode::High: return 8.0;
        default: return 0.0;
    }
}

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
    const auto effectiveDotMode = RadarData::EffectiveDotMatrixRenderMode(cfg.EnableDebugTools,
                                                                          cfg.DotRenderMode);
    return cfg.TerrainStyle == RadarData::TerrainRenderStyle::Texture
           || cfg.TerrainStyle == RadarData::TerrainRenderStyle::TextureAndDotMatrix
           || (cfg.TerrainStyle == RadarData::TerrainRenderStyle::DotMatrix
               && effectiveDotMode == RadarData::DotMatrixRenderMode::CachedTexture);
}

inline bool UsesDotMatrixTerrain(const RadarData::RadarConfig& cfg) {
    return cfg.TerrainStyle == RadarData::TerrainRenderStyle::DotMatrix
           || cfg.TerrainStyle == RadarData::TerrainRenderStyle::TextureAndDotMatrix;
}

inline bool UsesTerrainBoundaryLines(const RadarData::RadarConfig& cfg) {
    return UsesTextureTerrain(cfg) && cfg.ShowBoundaryEdges && cfg.WalkableMapBorderThickness > 0
           && cfg.BoundaryRenderMode == RadarData::TerrainBoundaryRenderMode::VectorLinesDebug;
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
                                const MapLayerProjection& largeMapProj,
                                TerrainTextureDrawStats* stats = nullptr) {
    if (stats) *stats = {};
    if (!dl) {
        if (stats) stats->skippedReason = "no draw list";
        return;
    }
    if (!terrain.Valid()) {
        if (stats) stats->skippedReason = "terrain texture invalid";
        return;
    }
    if (terrain.Width() <= 0 || terrain.Height() <= 0) {
        if (stats) stats->skippedReason = "terrain texture dimensions invalid";
        return;
    }
    if (!largeMapProj.valid) {
        if (stats) stats->skippedReason = "large map projection invalid";
        return;
    }

    const int cols = std::clamp(terrain.Width() / 20, 36, 72);
    const int rows = std::clamp(terrain.Height() / 20, 36, 72);
    const TerrainGridExtents extents = ResolveTerrainGridExtents(cfg, terrain);
    std::vector<ProjectedTerrainVertex> vertices;
    BuildProjectedTerrainVertexGrid(vertices, ctx, snap, cfg, terrain.Width(),
                                    terrain.Height(), cols, rows, extents.gxMin, extents.gyMin,
                                    extents.gxSpan, extents.gySpan, nullptr,
                                    &largeMapProj);
    constexpr float kTerrainTextureTintAlpha = 1.0f;
    if (stats) stats->tintAlpha = kTerrainTextureTintAlpha;

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

            if (stats) {
                const ImVec2 points[4] = {vtx0.pos, vtx1.pos, vtx2.pos, vtx3.pos};
                for (const ImVec2& point : points) {
                    if (!stats->rectValid) {
                        stats->minX = stats->maxX = point.x;
                        stats->minY = stats->maxY = point.y;
                        stats->rectValid = true;
                    } else {
                        stats->minX = std::min(stats->minX, point.x);
                        stats->minY = std::min(stats->minY, point.y);
                        stats->maxX = std::max(stats->maxX, point.x);
                        stats->maxY = std::max(stats->maxY, point.y);
                    }
                }
            }

            dl->AddImageQuad(terrain.TexRef(), vtx0.pos, vtx1.pos, vtx2.pos, vtx3.pos,
                             ImVec2(u0, v0), ImVec2(u1, v0), ImVec2(u1, v1),
                             ImVec2(u0, v1), IM_COL32_WHITE);
            if (stats) stats->submitted = true;
        }
    }

    if (stats && !stats->submitted && !stats->skippedReason)
        stats->skippedReason = "no valid projected terrain quads";
}

inline void DrawTerrainDotMatrix(ImDrawList* dl, PluginSDK::Context* ctx,
                                 const PluginSDK::Snapshot& snap,
                                 const WalkableBake& walkable,
                                 const RadarData::RadarConfig& cfg, bool useLargeMap,
                                 const MapLayerProjection* largeMapProj = nullptr,
                                 TerrainDetailDrawStats* stats = nullptr) {
    if (!dl || !ctx || !walkable.valid || walkable.width <= 0 || walkable.height <= 0
        || walkable.walkableMask.empty())
        return;

    const ImU32 fillColor =
        ImGui::ColorConvertFloat4ToU32(cfg.DotMatrixFillColor);

    const int step = std::max(1, cfg.DotCellStep);
    constexpr int kMaxDotProjectedCandidates = 40000;
    constexpr int kMaxDotDrawBudget = 18000;
    int effectiveStep = step;
    if (useLargeMap) {
        const double estimated = static_cast<double>(std::max(1, walkable.walkableCellCount))
                                 / static_cast<double>(step * step);
        if (estimated > kMaxDotProjectedCandidates) {
            const double ratio = estimated / static_cast<double>(kMaxDotProjectedCandidates);
            effectiveStep = step * std::max(1, static_cast<int>(std::ceil(std::sqrt(ratio))));
        }
    }
    if (stats) {
        stats->effectiveStep = effectiveStep;
        stats->adaptiveSamplingActive = effectiveStep != step;
    }
    const float halfSize = std::clamp(cfg.DotSize, 0.5f, 6.0f);
    const bool useAnchor = !useLargeMap;
    const TerrainAnchorDelta anchor =
        useAnchor ? ComputeTerrainPlayerAnchorDelta(ctx, snap, cfg, useLargeMap, walkable.width,
                                                    walkable.height)
                  : TerrainAnchorDelta{};

    for (int gy = 0; gy < walkable.height; gy += effectiveStep) {
        for (int gx = 0; gx < walkable.width; gx += effectiveStep) {
            const size_t idx = static_cast<size_t>(gy) * static_cast<size_t>(walkable.width)
                             + static_cast<size_t>(gx);
            if (idx >= walkable.walkableMask.size() || walkable.walkableMask[idx] == 0) continue;
            if (stats) ++stats->total;

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
            if (!projected) {
                if (stats) {
                    ++stats->culled;
                    ++stats->culledProjectionFailed;
                }
                continue;
            }

            if (useLargeMap) {
                if (!IsInsideMapRect(snap.LargeMap, sx, sy, 18.f)) {
                    if (stats) {
                        ++stats->culled;
                        ++stats->culledOutsideSurface;
                    }
                    continue;
                }
            } else if (!IsOnMinimapSurface(ctx, snap.MiniMap, sx, sy, 10.f)) {
                if (stats) {
                    ++stats->culled;
                    ++stats->culledOutsideSurface;
                }
                continue;
            }

            if (stats) ++stats->eligible;

            if (stats && stats->drawn >= kMaxDotDrawBudget) {
                stats->budgetHit = true;
                ++stats->culledBudget;
                return;
            }

            dl->AddRectFilled(ImVec2(sx - halfSize, sy - halfSize),
                              ImVec2(sx + halfSize, sy + halfSize), fillColor);
            if (stats) ++stats->drawn;
        }
    }
}

inline void DrawTerrainBoundaryLines(ImDrawList* dl, PluginSDK::Context* ctx,
                                     const PluginSDK::Snapshot& snap,
                                     const WalkableBake& walkable,
                                     const RadarData::RadarConfig& cfg, bool useLargeMap,
                                     const MapLayerProjection* largeMapProj = nullptr,
                                     TerrainDetailDrawStats* stats = nullptr) {
    if (!dl || !ctx || !walkable.valid || walkable.boundarySegments.empty()) return;
    const int targetDrawCount = ResolveBoundaryTargetDrawCount(cfg.BoundaryQuality);
    const double timeBudgetMs = ResolveBoundaryTimeBudgetMs(cfg.BoundaryQuality);
    if (stats) {
        stats->targetDrawCount = targetDrawCount;
        stats->timeBudgetMs = timeBudgetMs;
    }
    if (cfg.BoundaryQuality == RadarData::TerrainBoundaryQualityMode::Off) return;

    TerrainTexture terrainView;
    const TerrainGridExtents extents = ResolveTerrainGridExtents(cfg, terrainView);
    const ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(cfg.TextureWallEdgeColor);
    const float thickness = std::clamp(static_cast<float>(cfg.WalkableMapBorderThickness), 1.0f, 3.0f);
    const bool useAnchor = !useLargeMap;
    const TerrainAnchorDelta anchor =
        useAnchor ? ComputeTerrainPlayerAnchorDelta(ctx, snap, cfg, useLargeMap, walkable.width,
                                                    walkable.height)
                  : TerrainAnchorDelta{};
    const int stride = (targetDrawCount > 0 && !walkable.boundarySegments.empty())
                           ? std::max(1, static_cast<int>(std::ceil(
                                 static_cast<double>(walkable.boundarySegments.size())
                                 / static_cast<double>(targetDrawCount))))
                           : 1;
    if (stats) stats->stride = stride;
    const auto boundaryStart = std::chrono::steady_clock::now();

    for (size_t segmentIndex = 0; segmentIndex < walkable.boundarySegments.size(); ++segmentIndex) {
        const auto& segment = walkable.boundarySegments[segmentIndex];
        if (stats) ++stats->total;
        if (cfg.BoundaryScope == RadarData::TerrainBoundaryScopeMode::NearPlayerOnly
            && snap.Player.IsValid) {
            const float midX = (static_cast<float>(segment.x0) + static_cast<float>(segment.x1)) * 0.5f;
            const float midY = (static_cast<float>(segment.y0) + static_cast<float>(segment.y1)) * 0.5f;
            const float dx = midX - snap.Player.GridPositionX;
            const float dy = midY - snap.Player.GridPositionY;
            if ((dx * dx + dy * dy) > (cfg.BoundaryNearPlayerRadius * cfg.BoundaryNearPlayerRadius)) {
                if (stats) {
                    ++stats->culled;
                    ++stats->culledOutsideSurface;
                }
                continue;
            }
        }
        if (stride > 1 && (static_cast<int>(segmentIndex) % stride) != 0) {
            if (stats) ++stats->culled;
            continue;
        }
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
        if (!ok0 || !ok1) {
            if (stats) {
                ++stats->culled;
                ++stats->culledProjectionFailed;
            }
            continue;
        }

        if (stats) ++stats->eligible;

        if (useLargeMap) {
            if (stats && timeBudgetMs > 0.0 && (stats->drawn % 128) == 0 && stats->drawn > 0) {
                const double elapsedMs = std::chrono::duration<double, std::milli>(
                                             std::chrono::steady_clock::now() - boundaryStart)
                                             .count();
                if (elapsedMs >= timeBudgetMs) {
                    stats->budgetHit = true;
                    stats->timeBudgetHit = true;
                    ++stats->culledBudget;
                    return;
                }
            }
            dl->AddLine(ImVec2(sx0, sy0), ImVec2(sx1, sy1), lineColor, thickness);
            if (stats) ++stats->drawn;
            continue;
        }

        const ImVec2 p0(sx0, sy0);
        const ImVec2 p1(sx1, sy1);
        const ImVec2 mid((sx0 + sx1) * 0.5f, (sy0 + sy1) * 0.5f);
        if (!IsOnMinimapSurface(ctx, snap.MiniMap, p0.x, p0.y, 4.f)
            && !IsOnMinimapSurface(ctx, snap.MiniMap, p1.x, p1.y, 4.f)
            && !IsOnMinimapSurface(ctx, snap.MiniMap, mid.x, mid.y, 4.f)) {
            if (stats) {
                ++stats->culled;
                ++stats->culledOutsideSurface;
            }
            continue;
        }

        if (stats && timeBudgetMs > 0.0 && (stats->drawn % 128) == 0 && stats->drawn > 0) {
            const double elapsedMs = std::chrono::duration<double, std::milli>(
                                         std::chrono::steady_clock::now() - boundaryStart)
                                         .count();
            if (elapsedMs >= timeBudgetMs) {
                stats->budgetHit = true;
                stats->timeBudgetHit = true;
                ++stats->culledBudget;
                return;
            }
        }

        dl->AddLine(p0, p1, lineColor, thickness);
        if (stats) ++stats->drawn;
    }
}

class RadarOverlay {
public:
    RadarData::RadarConfig        cfg;
    RadarData::TargetDatabase     targets;
    RadarData::IconTables         icons;
    TerrainTexture                terrain;
    RadarPerf::AreaCacheState     cache;
    RadarPerf::OverlayPerfTiming  perf;
    PluginSDK::WalkableGridHandle walkable;
    RadarUi::UiBlockerDetector    uiBlocker;
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

    void Draw(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap) {
        if (!ShouldDraw(snap)) return;

        const bool perfCaptureEnabled = cfg.EnableDebugTools && cfg.EnablePerfTimingCapture;
        const auto drawStart = std::chrono::steady_clock::now();
        cache.BeginOverlayFrame(perfCaptureEnabled);
        uiBlocker.enabled = cfg.EnableUiBlockerDetection;

        const bool mapVisible = snap.LargeMap.IsVisible || snap.MiniMap.IsVisible;
        RadarPerf::OverlayPerfTiming::FrameFlags frameFlags{};
        frameFlags.mapVisible = mapVisible;
        frameFlags.debugToolsEnabled = cfg.EnableDebugTools;
        frameFlags.perfTimingCaptureEnabled = cfg.EnableDebugTools && cfg.EnablePerfTimingCapture;
        frameFlags.uiDiscoveryEnabled = cfg.EnableDebugTools && cfg.EnableUiDiscoveryTools;
        frameFlags.uiDiscoveryAutoRefreshEnabled = cfg.EnableDebugTools && cfg.EnableUiDiscoveryTools
                                                  && cfg.EnableUiDiscoveryAutoRefresh;
        frameFlags.pinnedWatchEnabled = cfg.EnableDebugTools && cfg.EnableUiDiscoveryTools
                                        && cfg.EnablePinnedUiWatch;
        const auto effectiveDotMode = RadarData::EffectiveDotMatrixRenderMode(cfg.EnableDebugTools,
                                                                              cfg.DotRenderMode);
        frameFlags.dotRenderMode = UsesDotMatrixTerrain(cfg)
                                       ? RadarData::DotMatrixRenderModeName(effectiveDotMode)
                                       : "Disabled";
        frameFlags.dotUserCellStep = cfg.DotCellStep;
        frameFlags.dotEffectiveCellStep = cfg.DotCellStep;
        frameFlags.boundaryRenderMode = RadarData::TerrainBoundaryRenderModeName(cfg.BoundaryRenderMode);
        frameFlags.boundaryQualityMode = RadarData::TerrainBoundaryQualityModeName(cfg.BoundaryQuality);
        frameFlags.boundaryScopeMode = RadarData::TerrainBoundaryScopeModeName(cfg.BoundaryScope);
        frameFlags.boundaryStride = 1;
        perf.BeginFrame(perfCaptureEnabled, frameFlags);
        if (!mapVisible) {
            mapWasVisible = false;
            perf.Record(RadarPerf::OverlayPerfTiming::Section::DrawUiTotal,
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - drawStart)
                            .count(),
                        true);
            perf.EndFrame();
            return;
        }
        if (!mapWasVisible) cache.InvalidatePoi();
        mapWasVisible = true;

        const auto blockerStart = std::chrono::steady_clock::now();
        const bool uiBlocked = uiBlocker.ShouldBlock(ctx, snap);
        perf.Record(RadarPerf::OverlayPerfTiming::Section::UiBlockerCheck,
                    std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - blockerStart)
                        .count(),
                    true);
        perf.flags.uiBlocked = uiBlocked;
        perf.flags.atlasBlocked = uiBlocked;
        perf.flags.uiBlockerRule = uiBlocked ? uiBlocker.match.ruleName : std::string{};
        perf.flags.uiBlockerReason = uiBlocked ? uiBlocker.match.reason : std::string{};
        perf.flags.uiBlockerRectValid = uiBlocked && uiBlocker.match.hasRect;
        perf.flags.uiBlockerRectX = uiBlocker.match.rectX;
        perf.flags.uiBlockerRectY = uiBlocker.match.rectY;
        perf.flags.uiBlockerRectW = uiBlocker.match.rectW;
        perf.flags.uiBlockerRectH = uiBlocker.match.rectH;
        perf.flags.uiBlockerOverlapValid = uiBlocked && uiBlocker.match.hasOverlap;
        perf.flags.uiBlockerOverlapX = uiBlocker.match.overlapX;
        perf.flags.uiBlockerOverlapY = uiBlocker.match.overlapY;
        perf.flags.uiBlockerOverlapW = uiBlocker.match.overlapW;
        perf.flags.uiBlockerOverlapH = uiBlocker.match.overlapH;
        perf.flags.uiBlockerOverlapArea = uiBlocker.match.overlapArea;
        if (uiBlocked) {
            perf.flags.heavyRebuildFrame = cache.frameSafety.heavyRebuildThisFrame;
            perf.flags.skippedOptionalDetail = cache.frameSafety.skipOptionalDetailThisFrame;
            perf.flags.optionalDetailSkipReason = cache.frameSafety.optionalDetailSkipReason;
            perf.Record(RadarPerf::OverlayPerfTiming::Section::DrawUiTotal,
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - drawStart)
                            .count(),
                        true);
            perf.EndFrame();
            return;
        }

        const auto areaCacheStart = std::chrono::steady_clock::now();
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
        perf.Record(RadarPerf::OverlayPerfTiming::Section::AreaCacheUpdate,
                    std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - areaCacheStart)
                        .count(),
                    true);
        constexpr double kHeavyAreaCacheThresholdMs = 16.0;
        constexpr double kHeavyRebuildSectionThresholdMs = 8.0;
        const double areaCacheUpdateMs = std::chrono::duration<double, std::milli>(
                                             std::chrono::steady_clock::now() - areaCacheStart)
                                             .count();
        if (areaCacheUpdateMs >= kHeavyAreaCacheThresholdMs)
            cache.frameSafety.PromoteHeavy("heavy area cache update");
        if (cache.perfDebug.walkableRebuildRan)
            perf.Record(RadarPerf::OverlayPerfTiming::Section::TerrainBakeRebuild,
                        cache.perfDebug.walkableRebuildMs, true);
        else
            perf.MarkSkipped(RadarPerf::OverlayPerfTiming::Section::TerrainBakeRebuild);
        if (cache.perfDebug.poiRebuildRan)
            perf.Record(RadarPerf::OverlayPerfTiming::Section::PoiCacheRebuild,
                        cache.perfDebug.poiRebuildMs, true);
        else
            perf.MarkSkipped(RadarPerf::OverlayPerfTiming::Section::PoiCacheRebuild);
        if (cache.perfDebug.entityRebuildRan)
            perf.Record(RadarPerf::OverlayPerfTiming::Section::EntityCacheRebuild,
                        cache.perfDebug.entityRebuildMs, true);
        else
            perf.MarkSkipped(RadarPerf::OverlayPerfTiming::Section::EntityCacheRebuild);
        if (cache.perfDebug.walkableRebuildMs >= kHeavyRebuildSectionThresholdMs)
            cache.frameSafety.PromoteHeavy("terrain rebuild");
        if (cache.perfDebug.poiRebuildMs >= kHeavyRebuildSectionThresholdMs)
            cache.frameSafety.PromoteHeavy("POI rebuild");
        if (cache.perfDebug.entityRebuildMs >= kHeavyRebuildSectionThresholdMs)
            cache.frameSafety.PromoteHeavy("entity rebuild");

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (!dl) {
            RadarData::RadarLog::Instance().Warn("DrawUI: no background draw list");
            perf.Record(RadarPerf::OverlayPerfTiming::Section::DrawUiTotal,
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - drawStart)
                            .count(),
                        true);
            perf.EndFrame();
            return;
        }

        if (snap.LargeMap.IsVisible) {
            bool terrainReady = false;
            TerrainDetailDrawStats dotStats;
            TerrainDetailDrawStats boundaryStats;
            bool boundaryTextureRebuilt = false;
            double boundaryTextureBuildMs = 0.0;
            bool dotTextureRebuilt = false;
            double dotTextureBuildMs = 0.0;
            TerrainTextureDrawStats terrainDrawStats;
            if (cfg.DrawWalkableMap && UsesTextureTerrain(cfg)) {
                const auto textureBuildStart = std::chrono::steady_clock::now();
                const bool textureWasCurrent = terrain.IsCurrent(ctx->D3DDevice, cache.walkable, cfg,
                                                                 snap.AreaChangeCounter, walkable.Data());
                const bool skipTerrainTextureBuild =
                    cache.frameSafety.skipOptionalDetailThisFrame
                    && !terrain.IsCurrent(ctx->D3DDevice, cache.walkable, cfg,
                                          snap.AreaChangeCounter, walkable.Data());
                if (skipTerrainTextureBuild) {
                    cache.frameSafety.skippedTerrainTextureBuildThisFrame = true;
                    perf.MarkSkipped(RadarPerf::OverlayPerfTiming::Section::TerrainTextureBuild);
                } else {
                    terrainReady = terrain.EnsureBuilt(ctx->D3DDevice, cache.walkable, cfg,
                                                      snap.AreaChangeCounter, walkable.Data());
                    boundaryTextureRebuilt = !textureWasCurrent && terrainReady
                                             && cfg.ShowBoundaryEdges
                                             && cfg.BoundaryRenderMode == RadarData::TerrainBoundaryRenderMode::CachedTexture
                                             && cfg.WalkableMapBorderThickness > 0;
                    dotTextureRebuilt = !textureWasCurrent && terrainReady
                                        && effectiveDotMode == RadarData::DotMatrixRenderMode::CachedTexture
                                        && UsesDotMatrixTerrain(cfg);
                    boundaryTextureBuildMs = std::chrono::duration<double, std::milli>(
                                                 std::chrono::steady_clock::now() - textureBuildStart)
                                                 .count();
                    dotTextureBuildMs = boundaryTextureBuildMs;
                    perf.Record(RadarPerf::OverlayPerfTiming::Section::TerrainTextureBuild,
                                boundaryTextureBuildMs,
                                true);
                }
            }
            const MapLayerProjection largeMapProj = BuildLargeMapLayerProjection(ctx, snap, cfg);
            MapClipScope clip(dl, snap.LargeMap, false);
            if (!cfg.DrawWalkableMap) {
                terrainDrawStats.skippedReason = "disabled setting";
            } else if (!UsesTextureTerrain(cfg)) {
                terrainDrawStats.skippedReason = "texture terrain mode disabled";
            } else if (!terrainReady) {
                terrainDrawStats.skippedReason = "terrain texture not ready";
            } else {
                DrawTerrainLargeMap(dl, ctx, snap, terrain, cfg, largeMapProj, &terrainDrawStats);
            }
            if (cfg.DrawWalkableMap && UsesDotMatrixTerrain(cfg)
                && effectiveDotMode == RadarData::DotMatrixRenderMode::VectorDotsDebug) {
                if (cache.frameSafety.skipOptionalDetailThisFrame) {
                    cache.frameSafety.skippedDotMatrixThisFrame = true;
                    perf.flags.dotSkipReason = cache.frameSafety.optionalDetailSkipReason;
                    perf.MarkSkipped(RadarPerf::OverlayPerfTiming::Section::DotMatrixDraw);
                } else {
                    const auto dotStart = std::chrono::steady_clock::now();
                    DrawTerrainDotMatrix(dl, ctx, snap, cache.walkable, cfg, true, &largeMapProj,
                                         &dotStats);
                    perf.Record(RadarPerf::OverlayPerfTiming::Section::DotMatrixDraw,
                                std::chrono::duration<double, std::milli>(
                                    std::chrono::steady_clock::now() - dotStart)
                                    .count(),
                                true);
                }
            } else {
                perf.flags.dotSkipReason = !UsesDotMatrixTerrain(cfg) ? "disabled setting"
                                          : (effectiveDotMode == RadarData::DotMatrixRenderMode::CachedTexture
                                                  ? "cached texture mode"
                                                  : "disabled setting");
                perf.MarkSkipped(RadarPerf::OverlayPerfTiming::Section::DotMatrixDraw);
            }
            if (cfg.DrawWalkableMap && UsesTerrainBoundaryLines(cfg)) {
                if (cache.frameSafety.skipOptionalDetailThisFrame) {
                    cache.frameSafety.skippedBoundaryLinesThisFrame = true;
                    perf.flags.boundarySkipReason = cache.frameSafety.optionalDetailSkipReason;
                    perf.MarkSkipped(RadarPerf::OverlayPerfTiming::Section::BoundaryLinesDraw);
                } else {
                    const auto boundaryStart = std::chrono::steady_clock::now();
                    DrawTerrainBoundaryLines(dl, ctx, snap, cache.walkable, cfg, true,
                                             &largeMapProj, &boundaryStats);
                    perf.Record(RadarPerf::OverlayPerfTiming::Section::BoundaryLinesDraw,
                                std::chrono::duration<double, std::milli>(
                                    std::chrono::steady_clock::now() - boundaryStart)
                                    .count(),
                                true);
                }
            } else {
                perf.flags.boundarySkipReason = cfg.BoundaryRenderMode == RadarData::TerrainBoundaryRenderMode::Off
                                                    ? "disabled render mode"
                                                    : "cached texture mode";
                perf.MarkSkipped(RadarPerf::OverlayPerfTiming::Section::BoundaryLinesDraw);
            }
            const int cachedDotCount = (cfg.DotRenderMode == RadarData::DotMatrixRenderMode::CachedTexture
                                        && UsesDotMatrixTerrain(cfg))
                                           ? (cache.walkable.walkableCellCount
                                              / std::max(1, cfg.DotCellStep * cfg.DotCellStep))
                                           : 0;
            const int effectiveCachedDotCount = (effectiveDotMode == RadarData::DotMatrixRenderMode::CachedTexture
                                                 && UsesDotMatrixTerrain(cfg))
                                                    ? (cache.walkable.walkableCellCount
                                                       / std::max(1, cfg.DotCellStep * cfg.DotCellStep))
                                                    : 0;
            perf.flags.dotCandidatesTotal = dotStats.total > 0 ? dotStats.total : effectiveCachedDotCount;
            perf.flags.dotUserCellStep = cfg.DotCellStep;
            perf.flags.dotEffectiveCellStep = dotStats.effectiveStep > 0 ? dotStats.effectiveStep
                                                                          : cfg.DotCellStep;
            perf.flags.dotAdaptiveSamplingActive = dotStats.adaptiveSamplingActive;
            perf.flags.dotCandidatesDrawn = dotStats.drawn > 0 ? dotStats.drawn : effectiveCachedDotCount;
            perf.flags.dotMatrixDotsRasterized = effectiveCachedDotCount;
            perf.flags.dotTextureRebuilt = dotTextureRebuilt;
            perf.flags.dotTextureBuildMs = dotTextureBuildMs;
            perf.flags.dotVectorDotsDrawn = dotStats.drawn;
            perf.flags.dotVectorDotsCulled = dotStats.culled;
            perf.flags.dotCandidatesCulled = dotStats.culled;
            perf.flags.dotCullProjectionFailed = dotStats.culledProjectionFailed;
            perf.flags.dotCullOutsideSurface = dotStats.culledOutsideSurface;
            perf.flags.dotCullBudget = dotStats.culledBudget;
            perf.flags.dotBudgetHit = dotStats.budgetHit;
            perf.flags.boundaryTargetDrawCount = boundaryStats.targetDrawCount;
            perf.flags.boundarySegmentsTotal = boundaryStats.total;
            perf.flags.boundarySegmentsEligible = boundaryStats.eligible;
            perf.flags.boundarySegmentsDrawn = boundaryStats.drawn;
            perf.flags.boundarySegmentsCulled = boundaryStats.culled;
            perf.flags.boundaryStride = boundaryStats.stride;
            perf.flags.boundaryBudgetHit = boundaryStats.budgetHit;
            perf.flags.boundaryTimeBudgetHit = boundaryStats.timeBudgetHit;
            perf.flags.boundaryTimeBudgetMs = boundaryStats.timeBudgetMs;
            const auto& terrainDebug = terrain.LastDebug();
            perf.flags.terrainTextureBuildCalled = terrainDebug.buildCalled;
            perf.flags.terrainTextureWidth = terrainDebug.width;
            perf.flags.terrainTextureHeight = terrainDebug.height;
            perf.flags.terrainTexturePixelsWritten = terrainDebug.pixelsWritten;
            perf.flags.terrainTextureNonTransparentPixels = terrainDebug.nonTransparentPixels;
            perf.flags.terrainTextureUploadCalled = terrainDebug.uploadCalled;
            perf.flags.terrainTextureUploadSucceeded = terrainDebug.uploadSucceeded;
            perf.flags.terrainTextureDrawSubmitted = terrainDrawStats.submitted;
            perf.flags.terrainTextureDrawSkippedReason = terrainDrawStats.skippedReason;
            perf.flags.terrainTextureDrawTintAlpha = terrainDrawStats.tintAlpha;
            perf.flags.terrainTextureDrawRectValid = terrainDrawStats.rectValid;
            perf.flags.terrainTextureDrawRectMinX = terrainDrawStats.minX;
            perf.flags.terrainTextureDrawRectMinY = terrainDrawStats.minY;
            perf.flags.terrainTextureDrawRectMaxX = terrainDrawStats.maxX;
            perf.flags.terrainTextureDrawRectMaxY = terrainDrawStats.maxY;
            if (cfg.ShowBoundaryEdges
                && cfg.BoundaryRenderMode == RadarData::TerrainBoundaryRenderMode::CachedTexture
                && cfg.WalkableMapBorderThickness > 0) {
                perf.flags.boundarySegmentsRasterized = static_cast<int>(cache.walkable.boundarySegments.size());
                perf.flags.boundaryTextureRebuilt = boundaryTextureRebuilt;
                perf.flags.boundaryTextureBuildMs = boundaryTextureBuildMs;
                perf.flags.boundarySkipReason = boundaryTextureRebuilt ? "cached texture rebuilt"
                                                                       : "cached texture reused";
            } else if (!cfg.ShowBoundaryEdges) {
                perf.flags.boundarySkipReason = "boundary edges disabled";
            }
            const auto entityDrawStart = std::chrono::steady_clock::now();
            cache.entities.Draw(ctx, snap, dl, &largeMapProj);
            perf.Record(RadarPerf::OverlayPerfTiming::Section::EntityDraw,
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - entityDrawStart)
                            .count(),
                        true);
            if (cfg.ShowImportantPOI) {
                cache.pois.UpdateScreenPositions(ctx, snap, &largeMapProj);
                const auto poiDrawStart = std::chrono::steady_clock::now();
                cache.pois.Draw(dl, cfg, cfg.EdgeIndicatorLargemap, cfg.EdgeIndicatorMinimap,
                                ctx, &snap);
                perf.Record(RadarPerf::OverlayPerfTiming::Section::PoiDraw,
                            std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - poiDrawStart)
                                .count(),
                            true);
                if (cache.frameSafety.skipOptionalDetailThisFrame) {
                    cache.frameSafety.skippedPoiEdgeIndicatorsThisFrame = true;
                } else {
                    cache.pois.DrawEdgeIndicators(ctx, dl, snap.LargeMap,
                                                  cfg.EdgeIndicatorLargemap, false);
                }
            } else {
                cache.pois.Clear();
            }
        } else if (snap.MiniMap.IsVisible) {
            MapClipScope clip(dl, snap.MiniMap, true);
            perf.flags.terrainTextureDrawSkippedReason = UsesTextureTerrain(cfg)
                                                            ? "cached terrain large-map only"
                                                            : "texture terrain mode disabled";
            if (cfg.DrawMiniMapEntities) {
                const auto entityDrawStart = std::chrono::steady_clock::now();
                cache.entities.Draw(ctx, snap, dl);
                perf.Record(RadarPerf::OverlayPerfTiming::Section::EntityDraw,
                            std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - entityDrawStart)
                                .count(),
                            true);
            } else {
                perf.MarkSkipped(RadarPerf::OverlayPerfTiming::Section::EntityDraw);
            }
            if (cfg.ShowImportantPOI) {
                cache.pois.UpdateScreenPositions(ctx, snap);
                const auto poiDrawStart = std::chrono::steady_clock::now();
                cache.pois.Draw(dl, cfg, cfg.EdgeIndicatorLargemap, cfg.EdgeIndicatorMinimap,
                                ctx, &snap);
                perf.Record(RadarPerf::OverlayPerfTiming::Section::PoiDraw,
                            std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - poiDrawStart)
                                .count(),
                            true);
                if (cache.frameSafety.skipOptionalDetailThisFrame) {
                    cache.frameSafety.skippedPoiEdgeIndicatorsThisFrame = true;
                } else {
                    cache.pois.DrawEdgeIndicators(ctx, dl, snap.MiniMap, cfg.EdgeIndicatorMinimap,
                                                  true);
                }
            } else {
                cache.pois.Clear();
            }
        }
        perf.flags.heavyRebuildFrame = cache.frameSafety.heavyRebuildThisFrame;
        perf.flags.skippedOptionalDetail = cache.frameSafety.skipOptionalDetailThisFrame;
        perf.flags.optionalDetailSkipReason = cache.frameSafety.optionalDetailSkipReason;
        perf.Record(RadarPerf::OverlayPerfTiming::Section::DrawUiTotal,
                    std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - drawStart)
                        .count(),
                    true);
        perf.EndFrame();
    }
};

} // namespace RadarRender
