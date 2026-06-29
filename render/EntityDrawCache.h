#pragma once

#include "EntityClassifier.h"
#include "EntityMarkers.h"
#include "MapProjection.h"
#include "data/RadarConfig.h"
#include "data/TargetDatabase.h"
#include "sdk/PluginSDK.h"

#include <imgui.h>
#include <unordered_map>
#include <vector>

namespace RadarRender {

inline constexpr float kLargeMapMarkerOffsetX = 2.0f;

struct EntityDrawCache {
    std::vector<RadarData::EntityDrawCmd> cmds;
    uint64_t                              lastSnapshotTime = 0;

    void Clear() {
        cmds.clear();
        lastSnapshotTime = 0;
    }

    static bool IsSelfEntity(const PluginSDK::Entity& e, const PluginSDK::Snapshot& snap) {
        if (!snap.Player.IsValid) return false;
        if (e.EntitySubtype == PluginSDK::EntitySubtype::PlayerSelf) return true;
        if (e.Address != 0 && e.Address == snap.Player.Address) return true;
        return false;
    }

    static bool AnyRuleUsesRuneshapeColor(const RadarData::IconTables& icons) {
        for (const auto& rule : icons.displayRules) {
            if (!rule.enabled || !rule.useRuneshapeColor) continue;
            if (RadarData::IsRuneshapeColourEligible(rule)) return true;
        }
        return false;
    }

    static ImU32 RuneshapeColorToImU32(uint32_t color) {
        const uint8_t r = static_cast<uint8_t>(color & 0xFFu);
        const uint8_t g = static_cast<uint8_t>((color >> 8) & 0xFFu);
        const uint8_t b = static_cast<uint8_t>((color >> 16) & 0xFFu);
        uint8_t a = static_cast<uint8_t>((color >> 24) & 0xFFu);
        if (a == 0) a = 255;
        return IM_COL32(r, g, b, a);
    }

    void Rebuild(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                 const RadarData::RadarConfig& cfg, const RadarData::TargetDatabase& db,
                 const RadarData::IconTables& icons) {
        cmds.clear();
        if (!ctx) return;
        cmds.reserve(static_cast<size_t>(std::min(cfg.MaxEntitiesDrawn, 2048)));

        std::unordered_map<uint32_t, ImU32> runeshapeColors;
        if (!cfg.UseLegacyClassifier && AnyRuleUsesRuneshapeColor(icons)) {
            for (const auto& runeshape : ctx->Runeshape.Runeshapes()) {
                if (runeshape.entityId == 0 || runeshape.entityId > 0xFFFFFFFFull) continue;
                runeshapeColors.emplace(static_cast<uint32_t>(runeshape.entityId),
                                        RuneshapeColorToImU32(runeshape.color));
            }
        }

        int count = 0;
        for (const auto& e : snap.Entities) {
            if (count >= cfg.MaxEntitiesDrawn) break;
            if (!e.IsValid) continue;
            if (IsSelfEntity(e, snap)) continue;
            if (e.EntityState == PluginSDK::EntityState::Useless) continue;
            if (cfg.HideOutsideNetworkBubble && e.Zone == PluginSDK::NearbyZone::Far) continue;
            if (e.EntityType == PluginSDK::EntityType::Monster && e.CurrentHP <= 0) continue;

            const std::string path = RadarData::NarrowPath(e.Path);
            if (!path.empty() && db.ignorePatterns.MatchesAny(path)) continue;

            RadarData::EntityDrawCmd cmd;
            cmd.gridX = e.GridPositionX;
            cmd.gridY = e.GridPositionY;
            cmd.terrainZ = e.TerrainHeight;

            const auto marker = ClassifyEntity(ctx, e, path, icons, cfg);
            if (!marker.matched || marker.hidden) continue;
            cmd.markerShape = marker.style.shape;
            cmd.markerSize = marker.style.size;
            cmd.markerColor = marker.style.color;
            if (marker.useRuneshapeColor) {
                if (const auto it = runeshapeColors.find(e.Id); it != runeshapeColors.end())
                    cmd.markerColor = it->second;
            }
            cmd.label = marker.style.label;
            cmds.push_back(cmd);
            ++count;
        }

        if (snap.Player.IsValid) {
            if (auto marker = ClassifySelf(icons, cfg)) {
                RadarData::EntityDrawCmd cmd;
                cmd.gridX = snap.Player.GridPositionX;
                cmd.gridY = snap.Player.GridPositionY;
                cmd.terrainZ = snap.Player.TerrainHeight;
                cmd.markerShape = marker->shape;
                cmd.markerSize = marker->size;
                cmd.markerColor = marker->color;
                cmd.label = marker->label;
                cmds.push_back(cmd);
            }
        }

        lastSnapshotTime = snap.LastUpdateTime;
    }

    void Draw(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap, ImDrawList* dl,
              const MapLayerProjection* largeMapProj = nullptr) const {
        if (!dl || !ctx) return;
        for (const auto& c : cmds) {
            if (c.markerShape == RadarData::MarkerShape::None || c.markerSize <= 0.f) continue;
            ProjectedScreen scr;
            if (snap.LargeMap.IsVisible && largeMapProj
                && largeMapProj->mode == RadarData::MapLayerProjectionMode::Unified2D) {
                scr = ProjectGridLargeMapLayer(*largeMapProj, ctx, snap, c.gridX, c.gridY,
                                               c.terrainZ, MapLayerSubject::Entity);
            } else {
                scr = ProjectEntityGridToScreen(ctx, snap, c.gridX, c.gridY, c.terrainZ);
            }
            if (!scr.valid) continue;
            const float drawX = scr.sx + (snap.LargeMap.IsVisible ? kLargeMapMarkerOffsetX : 0.0f);
            DrawEntityMarker(dl, c.markerShape, drawX, scr.sy, c.markerSize, c.markerColor);
            if (!c.label.empty())
                dl->AddText(ImVec2(drawX + c.markerSize + 4.f, scr.sy - c.markerSize - 2.f),
                            c.markerColor, c.label.c_str());
        }
    }
};

} // namespace RadarRender
