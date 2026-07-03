#pragma once

#include "EntityClassifier.h"
#include "EntityMarkers.h"
#include "MapProjection.h"
#include "data/RadarConfig.h"
#include "data/TargetDatabase.h"
#include "sdk/PluginSDK.h"

#include <imgui.h>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace RadarRender {

inline constexpr float kLargeMapMarkerOffsetX = 2.0f;

struct EntityDrawCache {
    struct RememberedMarker {
        std::string             ruleId;
        size_t                  ruleIndex = SIZE_MAX;
        float                   gridX = 0.f;
        float                   gridY = 0.f;
        float                   terrainZ = 0.f;
        RadarData::MarkerShape  markerShape = RadarData::MarkerShape::None;
        float                   markerSize = 0.f;
        ImU32                   markerColor = 0;
        std::string             label;
    };

    std::vector<RadarData::EntityDrawCmd> cmds;
    std::unordered_map<std::string, RememberedMarker> remembered;
    std::unordered_set<std::string>                  liveRememberedKeys;
    uint64_t                              lastSnapshotTime = 0;

    void Clear() {
        cmds.clear();
        remembered.clear();
        liveRememberedKeys.clear();
        lastSnapshotTime = 0;
    }

    static int QuantizeRememberedGrid(float value) {
        return static_cast<int>(std::floor(value * 2.f + (value >= 0.f ? 0.5f : -0.5f)));
    }

    static std::string BuildRememberedKey(const PluginSDK::Entity& e, std::string_view path,
                                          const RadarData::DisplayRule* matchedRule,
                                          size_t matchedRuleIndex) {
        std::string key = matchedRule && !matchedRule->id.empty()
                              ? matchedRule->id
                              : std::string("rule#") + std::to_string(matchedRuleIndex);
        key += '|';
        key += path;
        key += '|';
        key += RadarData::NarrowPath(e.PlayerName);
        key += '|';
        if (e.Address != 0) {
            key += 'a';
            key += std::to_string(static_cast<unsigned long long>(e.Address));
        } else {
            key += 'g';
            key += std::to_string(QuantizeRememberedGrid(e.GridPositionX));
            key += ',';
            key += std::to_string(QuantizeRememberedGrid(e.GridPositionY));
        }
        return key;
    }

    static bool RuleAllowsRemembered(const RememberedMarker& rememberedMarker,
                                     const RadarData::IconTables& icons) {
        const RadarData::DisplayRule* rule = nullptr;
        if (!rememberedMarker.ruleId.empty()) {
            for (const auto& candidate : icons.displayRules) {
                if (candidate.id == rememberedMarker.ruleId) {
                    rule = &candidate;
                    break;
                }
            }
        } else if (rememberedMarker.ruleIndex < icons.displayRules.size()) {
            rule = &icons.displayRules[rememberedMarker.ruleIndex];
        }
        return rule && rule->enabled && rule->rememberUntilZone;
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
        liveRememberedKeys.clear();
        if (!ctx) return;
        cmds.reserve(static_cast<size_t>(std::min(cfg.MaxEntitiesDrawn, 2048)));

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
            cmd.label = marker.style.label;

            if (!cfg.UseLegacyClassifier && marker.matchedRule
                && marker.matchedRule->rememberUntilZone
                && cmd.markerShape != RadarData::MarkerShape::None && cmd.markerSize > 0.f) {
                const std::string key =
                    BuildRememberedKey(e, path, marker.matchedRule, marker.matchedRuleIndex);
                liveRememberedKeys.insert(key);
                remembered[key] = RememberedMarker{marker.matchedRule->id,
                                                   marker.matchedRuleIndex,
                                                   cmd.gridX,
                                                   cmd.gridY,
                                                   cmd.terrainZ,
                                                   cmd.markerShape,
                                                   cmd.markerSize,
                                                   cmd.markerColor,
                                                   cmd.label};
            }

            cmds.push_back(cmd);
            ++count;
        }

        for (auto it = remembered.begin(); it != remembered.end();) {
            if (!RuleAllowsRemembered(it->second, icons)) {
                it = remembered.erase(it);
                continue;
            }
            const auto& key = it->first;
            const auto& rememberedMarker = it->second;
            if (liveRememberedKeys.contains(key)) {
                ++it;
                continue;
            }
            if (rememberedMarker.markerShape == RadarData::MarkerShape::None
                || rememberedMarker.markerSize <= 0.f) {
                ++it;
                continue;
            }
            RadarData::EntityDrawCmd cmd;
            cmd.gridX = rememberedMarker.gridX;
            cmd.gridY = rememberedMarker.gridY;
            cmd.terrainZ = rememberedMarker.terrainZ;
            cmd.markerShape = rememberedMarker.markerShape;
            cmd.markerSize = rememberedMarker.markerSize;
            cmd.markerColor = rememberedMarker.markerColor;
            cmd.label = rememberedMarker.label;
            cmds.push_back(std::move(cmd));
            ++it;
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
