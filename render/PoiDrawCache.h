#pragma once

#include "IconAtlas.h"
#include "MapProjection.h"
#include "data/PathMatcher.h"
#include "data/RadarConfig.h"
#include "data/TargetDatabase.h"
#include "data/IconTables.h"
#include "sdk/PluginSDK.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <optional>
#include <unordered_map>
#include <vector>

namespace RadarRender {

inline constexpr float kLargeMapPoiOffsetX = 2.0f;

struct PoiDrawCache {
    std::vector<RadarData::PoiResolved> pois;

    static RadarData::IconDef ResolvePoiIcon(const RadarData::TargetEntry& t,
                                             const RadarData::IconTables& icons) {
        if (!t.iconName.empty()) {
            const auto findIn = [&](const auto& m) -> std::optional<RadarData::IconDef> {
                if (auto it = m.find(t.iconName); it != m.end()) return it->second;
                return std::nullopt;
            };
            if (auto d = findIn(icons.baseIcons)) return *d;
            if (auto d = findIn(icons.chestIcons)) return *d;
            if (auto d = findIn(icons.breachIcons)) return *d;
            if (auto d = findIn(icons.deliriumIcons)) return *d;
            if (auto d = findIn(icons.expeditionIcons)) return *d;
        }
        return icons.otherImportantDefault;
    }

    void Clear() { pois.clear(); }

    static int CountMatchingTgtLocations(
        PluginSDK::Context* ctx, const std::vector<const RadarData::TargetEntry*>& targets) {
        if (!ctx || targets.empty()) return 0;
        std::vector<RadarData::CompiledPattern> compiled;
        compiled.reserve(targets.size());
        for (const auto* t : targets) compiled.push_back(RadarData::CompilePattern(t->path));
        return CountMatchingTgtLocations(ctx, compiled);
    }

    static int CountMatchingTgtLocations(
        PluginSDK::Context* ctx,
        const std::vector<RadarData::CompiledPattern>& compiled) {
        if (!ctx || compiled.empty()) return 0;
        int matches = 0;
        ctx->Terrain.EnumerateTgtLocations([&](const PluginSDK::TgtLocation& loc) {
            const auto cand = RadarData::CompileCandidate(loc.Path);
            for (const auto& pat : compiled) {
                if (RadarData::MatchPattern(pat, cand)) {
                    ++matches;
                    break;
                }
            }
            return true;
        });
        return matches;
    }

    static int CountMatchingEntities(
        const PluginSDK::Snapshot& snap,
        const std::vector<const RadarData::TargetEntry*>& targets) {
        if (targets.empty()) return 0;
        std::vector<RadarData::CompiledPattern> compiled;
        compiled.reserve(targets.size());
        for (const auto* t : targets) compiled.push_back(RadarData::CompilePattern(t->path));
        return CountMatchingEntities(snap, compiled);
    }

    static int CountMatchingEntities(
        const PluginSDK::Snapshot& snap,
        const std::vector<RadarData::CompiledPattern>& compiled) {
        if (compiled.empty()) return 0;
        int matches = 0;
        for (const auto& e : snap.Entities) {
            if (!e.IsValid) continue;
            const auto cand = RadarData::CompileCandidate(std::wstring_view(e.Path));
            if (cand.normalized.empty()) continue;
            for (const auto& pat : compiled) {
                if (RadarData::MatchPattern(pat, cand)) {
                    ++matches;
                    break;
                }
            }
        }
        return matches;
    }

    // TGT metatiles are 3x3; averaging all cells pulls the map marker south. Use the north row.
    static bool ProjectMetatileScreenNorth(PluginSDK::Context* ctx,
                                           const PluginSDK::Snapshot& snap,
                                           const std::vector<std::pair<float, float>>& cells,
                                           float& outSx, float& outSy,
                                           const MapLayerProjection* largeMapProj = nullptr) {
        if (!ctx || cells.empty()) return false;
        float minGy = cells[0].second;
        for (const auto& [gx, gy] : cells) {
            (void)gx;
            minGy = std::min(minGy, gy);
        }
        float sx = 0.f, sy = 0.f;
        int   n = 0;
        for (const auto& [gx, gy] : cells) {
            if (gy > minGy + 0.5f) continue;
            ProjectedScreen scr;
            if (snap.LargeMap.IsVisible) {
                const float tz = TerrainHeightAtGrid(ctx, gx, gy);
                if (largeMapProj) {
                    scr = ProjectGridLargeMapLayer(*largeMapProj, ctx, snap, gx, gy, tz,
                                                   MapLayerSubject::TargetPoi);
                } else {
                    scr = ProjectGridToLargeMapScreen(ctx, snap, gx, gy, tz);
                }
            } else if (snap.MiniMap.IsVisible) {
                const float tz = TerrainHeightAtGrid(ctx, gx, gy);
                scr = ProjectGridToMiniMapScreen(ctx, snap, gx, gy, tz);
            } else {
                continue;
            }
            if (!scr.valid) continue;
            sx += scr.sx;
            sy += scr.sy;
            ++n;
        }
        if (n == 0) return false;
        outSx = sx / static_cast<float>(n);
        outSy = sy / static_cast<float>(n);
        return true;
    }

    static void FillMetatileCells(PluginSDK::Context* ctx, RadarData::PoiResolved& p,
                                  const RadarData::TargetEntry& t,
                                  const RadarData::CompiledPattern& pat) {
        if (!ctx || !t.hasAnchor) return;
        p.metatileCells.clear();
        ctx->Terrain.EnumerateTgtLocations([&](const PluginSDK::TgtLocation& loc) {
            const auto cand = RadarData::CompileCandidate(loc.Path);
            if (cand.normalized != pat.normalized) return true;
            if (t.anchorTileX != 0 || t.anchorTileY != 0) {
                if (std::abs(loc.TileX - t.anchorTileX) > 1 || std::abs(loc.TileY - t.anchorTileY) > 1)
                    return true;
            } else if (std::hypot(loc.X - t.anchorGridX, loc.Y - t.anchorGridY) > 40.f) {
                return true;
            }
            p.metatileCells.emplace_back(loc.X, loc.Y);
            return true;
        });
        if (p.metatileCells.size() < 4) {
            p.metatileCells.clear();
            return;
        }
        float cx = 0.f, cy = 0.f;
        for (const auto& [gx, gy] : p.metatileCells) {
            cx += gx;
            cy += gy;
        }
        const float inv = 1.f / static_cast<float>(p.metatileCells.size());
        p.gridX = cx * inv;
        p.gridY = cy * inv;
        p.terrainZ = ctx->Terrain.GetTerrainHeight(static_cast<int>(p.gridX), static_cast<int>(p.gridY));
    }

    void UpdateScreenPositions(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                               const MapLayerProjection* largeMapProj = nullptr) {
        for (auto& p : pois) {
            p.hasScreen = false;
            ProjectedScreen scr;
            if (p.fromTgt && p.metatileCells.size() >= 4) {
                float northSx = 0.f, northSy = 0.f;
                if (ProjectMetatileScreenNorth(ctx, snap, p.metatileCells, northSx, northSy,
                                               largeMapProj)) {
                    p.screenX = northSx;
                    p.screenY = northSy;
                    p.hasScreen = true;
                    continue;
                }
            }
            if (p.fromTgt) {
                if (snap.LargeMap.IsVisible) {
                    if (largeMapProj) {
                        scr = ProjectGridLargeMapLayer(*largeMapProj, ctx, snap, p.gridX,
                                                       p.gridY, p.terrainZ,
                                                       MapLayerSubject::TargetPoi);
                    } else {
                        scr = ProjectTgtToLargeMapScreen(ctx, snap, p.gridX, p.gridY);
                    }
                    if (scr.valid) {
                        p.screenX = scr.sx;
                        p.screenY = scr.sy;
                        p.hasScreen = true;
                        continue;
                    }
                }
                if (snap.MiniMap.IsVisible) {
                    scr = ProjectTgtToMiniMapScreen(ctx, snap, p.gridX, p.gridY);
                    if (scr.valid) {
                        p.screenX = scr.sx;
                        p.screenY = scr.sy;
                        p.hasScreen = true;
                        if (snap.Player.IsValid) {
                            const auto ps = ProjectTgtToMiniMapScreen(
                                ctx, snap, snap.Player.GridPositionX, snap.Player.GridPositionY);
                            if (ps.valid) {
                                const float dx = p.screenX - ps.sx;
                                const float dy = p.screenY - ps.sy;
                                if ((dx * dx + dy * dy) < (14.f * 14.f)) p.hasScreen = false;
                            }
                        }
                    }
                }
            } else {
                if (snap.LargeMap.IsVisible) {
                    if (largeMapProj) {
                        scr = ProjectGridLargeMapLayer(*largeMapProj, ctx, snap, p.gridX,
                                                       p.gridY, p.terrainZ,
                                                       MapLayerSubject::Poi);
                    } else {
                        scr = ProjectGridToLargeMapScreen(ctx, snap, p.gridX, p.gridY,
                                                          p.terrainZ);
                    }
                    if (scr.valid) {
                        p.screenX = scr.sx;
                        p.screenY = scr.sy;
                        p.hasScreen = true;
                        continue;
                    }
                }
                if (snap.MiniMap.IsVisible) {
                    scr = ProjectGridToMiniMapScreen(ctx, snap, p.gridX, p.gridY, p.terrainZ);
                    if (scr.valid) {
                        p.screenX = scr.sx;
                        p.screenY = scr.sy;
                        p.hasScreen = true;
                    }
                }
            }
        }
    }

    void Rebuild(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                 const RadarData::RadarConfig& cfg, const RadarData::TargetDatabase& db,
                 const RadarData::IconTables& icons) {
        Clear();
        if (!ctx || !cfg.ShowImportantPOI) return;

        const auto targets = db.GetTargetsForArea(snap.CurrentAreaHash, snap.CurrentAreaName);
        if (targets.empty()) return;

        std::vector<RadarData::CompiledPattern> compiled;
        compiled.reserve(targets.size());
        for (const auto* t : targets)
            compiled.push_back(RadarData::CompilePattern(t->path));

        struct TgtCand {
            float gx = 0.f;
            float gy = 0.f;
        };
        constexpr float kPoiClusterDist = 120.f;
        auto clusterCands = [](const std::vector<TgtCand>& cands, float maxDist) {
            std::vector<std::vector<TgtCand>> clusters;
            for (const TgtCand& c : cands) {
                int best = -1;
                float bestDist = maxDist;
                for (size_t ci = 0; ci < clusters.size(); ++ci) {
                    float cx = 0.f, cy = 0.f;
                    for (const TgtCand& m : clusters[ci]) {
                        cx += m.gx;
                        cy += m.gy;
                    }
                    const float inv = 1.f / static_cast<float>(clusters[ci].size());
                    cx *= inv;
                    cy *= inv;
                    const float d = std::hypot(c.gx - cx, c.gy - cy);
                    if (d < bestDist) {
                        bestDist = d;
                        best = static_cast<int>(ci);
                    }
                }
                if (best >= 0)
                    clusters[static_cast<size_t>(best)].push_back(c);
                else
                    clusters.push_back({c});
            }
            return clusters;
        };
        std::vector<std::vector<TgtCand>> perTarget(targets.size());

        ctx->Terrain.EnumerateTgtLocations([&](const PluginSDK::TgtLocation& loc) {
            const auto cand = RadarData::CompileCandidate(loc.Path);
            for (size_t i = 0; i < targets.size(); ++i) {
                if (!RadarData::MatchPattern(compiled[i], cand)) continue;
                const auto* t = targets[i];
                if (t->hasAnchor) {
                    if (t->anchorTileX != 0 || t->anchorTileY != 0) {
                        if (std::abs(loc.TileX - t->anchorTileX) > 1
                            || std::abs(loc.TileY - t->anchorTileY) > 1)
                            continue;
                    } else if (std::hypot(loc.X - t->anchorGridX, loc.Y - t->anchorGridY) > 40.f) {
                        continue;
                    }
                }
                perTarget[i].push_back({loc.X, loc.Y});
            }
            return true;
        });

        constexpr float kMinPoiGridSep = 80.f;

        for (size_t i = 0; i < targets.size(); ++i) {
            if (perTarget[i].empty()) continue;
            const auto* t = targets[i];

            struct ScoredCand {
                float gx = 0.f;
                float gy = 0.f;
                float score = 0.f;
            };
            std::vector<ScoredCand> scored;
            const auto clusters = clusterCands(perTarget[i], kPoiClusterDist);
            scored.reserve(clusters.size());
            for (const auto& cluster : clusters) {
                float cCentX = 0.f, cCentY = 0.f;
                for (const TgtCand& c : cluster) {
                    cCentX += c.gx;
                    cCentY += c.gy;
                }
                const float invN = 1.f / static_cast<float>(cluster.size());
                cCentX *= invN;
                cCentY *= invN;

                ScoredCand best = {cluster[0].gx, cluster[0].gy, -1e9f};
                for (const TgtCand& c : cluster) {
                    float score = -std::hypot(c.gx - cCentX, c.gy - cCentY);
                    if (ctx->Terrain.IsWalkable(static_cast<int>(c.gx), static_cast<int>(c.gy)))
                        score += 500.f;
                    if (snap.LargeMap.IsVisible || snap.MiniMap.IsVisible) {
                        if (ProjectTgtToMapScreen(ctx, snap, c.gx, c.gy).valid) score += 5000.f;
                    }
                    if (score > best.score) best = {c.gx, c.gy, score};
                }
                scored.push_back(best);
            }
            std::sort(scored.begin(), scored.end(),
                      [](const ScoredCand& a, const ScoredCand& b) { return a.score > b.score; });

            if (scored.size() > 1) {
                size_t bestIdx = 0;
                if (t->hasAnchor) {
                    float bestD = 1e9f;
                    for (size_t ri = 0; ri < scored.size(); ++ri) {
                        const float d = std::hypot(scored[ri].gx - t->anchorGridX,
                                                   scored[ri].gy - t->anchorGridY);
                        if (d < bestD) {
                            bestD = d;
                            bestIdx = ri;
                        }
                    }
                } else if (snap.Player.IsValid) {
                    const float top = scored[0].score;
                    std::vector<size_t> tied;
                    for (size_t ri = 0; ri < scored.size(); ++ri) {
                        if (scored[ri].score >= top - 0.5f) tied.push_back(ri);
                        else break;
                    }
                    if (tied.size() > 1) {
                        float bestD = 1e9f;
                        for (size_t ri : tied) {
                            const float d = std::hypot(scored[ri].gx - snap.Player.GridPositionX,
                                                       scored[ri].gy - snap.Player.GridPositionY);
                            if (d < bestD) {
                                bestD = d;
                                bestIdx = ri;
                            }
                        }
                    }
                }
                if (bestIdx != 0) std::swap(scored[0], scored[bestIdx]);
            }

            const int want = std::clamp(t->expectedCount, 1, 32);
            int placed = 0;
            for (const ScoredCand& sc : scored) {
                if (placed >= want) break;
                bool tooClose = false;
                for (const auto& existing : pois) {
                    const float sep = std::hypot(sc.gx - existing.gridX, sc.gy - existing.gridY);
                    if (sep < 0.5f) continue;
                    if (sep < kMinPoiGridSep) {
                        tooClose = true;
                        break;
                    }
                }
                if (tooClose) continue;

                RadarData::PoiResolved p;
                p.name = t->name;
                p.gridX = sc.gx;
                p.gridY = sc.gy;
                p.terrainZ = 0.f;
                p.fromTgt = true;
                p.showIcon = t->showIcon;
                p.iconSize = t->iconSize > 0 ? t->iconSize : 30.f;
                const auto iconDef = ResolvePoiIcon(*t, icons);
                p.iconCx = iconDef.cx;
                p.iconCy = iconDef.cy;
                p.nameColor = t->nameColor;
                p.bgColor = t->bgColor;
                FillMetatileCells(ctx, p, *t, compiled[i]);
                pois.push_back(std::move(p));
                ++placed;
            }
        }

        for (const auto& e : snap.Entities) {
            if (!e.IsValid) continue;
            const auto cand = RadarData::CompileCandidate(std::wstring_view(e.Path));
            if (cand.normalized.empty()) continue;
            for (size_t i = 0; i < targets.size(); ++i) {
                if (!RadarData::MatchPattern(compiled[i], cand)) continue;
                const auto* t = targets[i];
                RadarData::PoiResolved p;
                p.name = t->name;
                p.gridX = e.GridPositionX;
                p.gridY = e.GridPositionY;
                p.terrainZ = e.TerrainHeight;
                p.fromTgt = false;
                p.showIcon = t->showIcon;
                p.iconSize = t->iconSize > 0 ? t->iconSize : 30.f;
                const auto iconDef = ResolvePoiIcon(*t, icons);
                p.iconCx = iconDef.cx;
                p.iconCy = iconDef.cy;
                p.nameColor = t->nameColor;
                p.bgColor = t->bgColor;
                pois.push_back(std::move(p));
            }
        }
    }

    void Draw(ImDrawList* dl, const IconAtlas& atlas, const RadarData::RadarConfig& cfg,
              bool edgeLarge, bool edgeMini, PluginSDK::Context* ctx = nullptr,
              const PluginSDK::Snapshot* snap = nullptr) {
        if (!dl) return;
        (void)ctx;
        (void)snap;
        for (const auto& p : pois) {
            if (!p.hasScreen) continue;
            const ImU32 nameCol = p.nameColor.ToImU32();
            const float drawX = p.screenX + ((snap && snap->LargeMap.IsVisible) ? kLargeMapPoiOffsetX : 0.0f);

            const bool drawIcon = cfg.DrawPoiIcons && p.showIcon && atlas.Valid();
            if (drawIcon) {
                atlas.DrawIcon(dl, p.iconCx, p.iconCy, p.iconSize, drawX, p.screenY, nameCol);
            } else {
                dl->AddCircleFilled(ImVec2(drawX, p.screenY), 4.f, nameCol);
            }

            const char* label = p.name.c_str();
            ImVec2 ts = ImGui::CalcTextSize(label);
            constexpr float kLabelOffsetX = 10.f;
            ImVec2 pos(drawX - ts.x * 0.5f + kLabelOffsetX, p.screenY - ts.y - 8.f);
            if (cfg.EnablePOIBackground)
                dl->AddRectFilled(ImVec2(pos.x - 2, pos.y - 1),
                                  ImVec2(pos.x + ts.x + 2, pos.y + ts.y + 1),
                                  p.bgColor.ToImU32());
            dl->AddLine(ImVec2(drawX, p.screenY), ImVec2(pos.x + ts.x * 0.5f, pos.y + ts.y),
                        IM_COL32(255, 255, 255, 180), 1.f);
            dl->AddText(pos, nameCol, label);
        }
        (void)edgeLarge;
        (void)edgeMini;
    }

    void DrawEdgeIndicators(PluginSDK::Context* ctx, ImDrawList* dl,
                            const PluginSDK::MapData& map, bool enabled,
                            bool minimap) const {
        if (!dl || !enabled || !map.IsVisible) return;
        const float margin = 8.f;

        for (const auto& p : pois) {
            if (!p.hasScreen) continue;
            if (IsInsideMapViewport(ctx, map, p.screenX, p.screenY, minimap)) continue;

            float sx = p.screenX;
            float sy = p.screenY;
            if (minimap) {
                const float radius = std::min(map.SizeX, map.SizeY) * 0.5f - margin;
                if (radius > 1.f) {
                    float clipCx = map.CenterX, clipCy = map.CenterY;
                    GetMinimapClipOrigin(ctx, map, clipCx, clipCy);
                    const float dx = sx - clipCx;
                    const float dy = sy - clipCy;
                    const float len = std::sqrt(dx * dx + dy * dy);
                    if (len > 0.001f) {
                        sx = clipCx + dx * (radius / len);
                        sy = clipCy + dy * (radius / len);
                    }
                }
            } else {
                const float halfW = map.SizeX * 0.5f;
                const float halfH = map.SizeY * 0.5f;
                const float minX = map.CenterX - halfW + margin;
                const float maxX = map.CenterX + halfW - margin;
                const float minY = map.CenterY - halfH + margin;
                const float maxY = map.CenterY + halfH - margin;
                sx = std::clamp(sx, minX, maxX);
                sy = std::clamp(sy, minY, maxY);
            }
            dl->AddCircleFilled(ImVec2(sx, sy), 5.f, p.nameColor.ToImU32());
        }
    }
};

} // namespace RadarRender
