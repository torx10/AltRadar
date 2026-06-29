#pragma once

#include "render/EntityDrawCache.h"
#include "render/PoiDrawCache.h"
#include "render/WalkableBake.h"
#include "data/RadarConfig.h"
#include "data/TargetDatabase.h"
#include "data/IconTables.h"
#include "sdk/PluginSDK.h"

#include <chrono>

namespace RadarPerf {

struct AreaCacheState {
    uint64_t                        areaCounter = 0;
    const uint8_t*                  walkablePtr = nullptr;
    RadarRender::WalkableBake       walkable;
    RadarRender::EntityDrawCache    entities;
    RadarRender::PoiDrawCache       pois;
    uint64_t                        entitySnapshotTime = 0;
    bool                            poiDirty = true;
    int                             lastTgtMatchCount = -1;
    int                             lastEntityMatchCount = -1;
    std::chrono::steady_clock::time_point lastTgtPollTimePoint = std::chrono::steady_clock::now();
    std::string                     lastTgtAreaKey;
    std::vector<RadarData::CompiledPattern> lastTargetPatterns;
    std::vector<const RadarData::TargetEntry*> lastTargets;

    void Clear() {
        areaCounter = 0;
        walkablePtr = nullptr;
        walkable.Clear();
        entities.Clear();
        pois.Clear();
        entitySnapshotTime = 0;
        poiDirty = true;
        lastTgtMatchCount = -1;
        lastEntityMatchCount = -1;
        lastTgtPollTimePoint = std::chrono::steady_clock::now();
        lastTgtAreaKey.clear();
        lastTargetPatterns.clear();
        lastTargets.clear();
    }

    bool NeedsFullRebuild(const PluginSDK::Snapshot& snap, const uint8_t* walkData) const {
        return snap.AreaChangeCounter != areaCounter || walkData != walkablePtr;
    }

    bool NeedsEntityRebuild(const PluginSDK::Snapshot& snap) const {
        return snap.LastUpdateTime != entitySnapshotTime;
    }

    bool RefreshTargetPatternCache(const PluginSDK::Snapshot& snap,
                                   const RadarData::TargetDatabase& db) {
        const auto targets = db.GetTargetsForArea(snap.CurrentAreaHash, snap.CurrentAreaName);
        const auto areaKey = db.ResolveAreaKey(snap.CurrentAreaHash, snap.CurrentAreaName);
        bool rebuildPatterns = areaKey != lastTgtAreaKey || targets.size() != lastTargets.size();
        if (!rebuildPatterns) {
            for (size_t i = 0; i < targets.size(); ++i) {
                if (targets[i] != lastTargets[i]) {
                    rebuildPatterns = true;
                    break;
                }
            }
        }

        if (rebuildPatterns) {
            lastTgtAreaKey = areaKey;
            lastTargets = targets;
            lastTargetPatterns.clear();
            lastTargetPatterns.reserve(targets.size());
            for (const auto* t : targets)
                lastTargetPatterns.push_back(RadarData::CompilePattern(t->path));
        }

        if (targets.empty()) {
            lastTgtAreaKey = areaKey;
            lastTargets.clear();
            lastTargetPatterns.clear();
        }

        return !lastTargetPatterns.empty();
    }

    void RefreshPoiMatchCounts(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap) {
        if (!ctx || lastTargetPatterns.empty()) {
            lastTgtMatchCount = -1;
            lastEntityMatchCount = -1;
            return;
        }
        lastTgtMatchCount = RadarRender::PoiDrawCache::CountMatchingTgtLocations(ctx,
                                                                                 lastTargetPatterns);
        lastEntityMatchCount = RadarRender::PoiDrawCache::CountMatchingEntities(snap,
                                                                                 lastTargetPatterns);
    }

    void RebuildAll(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                    PluginSDK::WalkableGridHandle& gridHandle,
                    const RadarData::RadarConfig& cfg, const RadarData::TargetDatabase& db,
                            const RadarData::IconTables& icons) {
        areaCounter = snap.AreaChangeCounter;
        walkablePtr = gridHandle.Data();
        walkable.Rebuild(ctx, gridHandle, cfg);
        pois.Rebuild(ctx, snap, cfg, db, icons);
        entities.Rebuild(ctx, snap, cfg, db, icons);
        entitySnapshotTime = snap.LastUpdateTime;
        poiDirty = false;
        if (cfg.ShowImportantPOI && RefreshTargetPatternCache(snap, db))
            RefreshPoiMatchCounts(ctx, snap);
        else {
            lastTgtMatchCount = -1;
            lastEntityMatchCount = -1;
        }
    }

    void RebuildEntitiesOnly(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                             const RadarData::RadarConfig& cfg,
                             const RadarData::TargetDatabase& db,
                            const RadarData::IconTables& icons) {
        entities.Rebuild(ctx, snap, cfg, db, icons);
        entitySnapshotTime = snap.LastUpdateTime;
    }

    void InvalidatePoi() { poiDirty = true; }

    // Rebuild when TGT tiles or matching entities appear (e.g. another Obelisk).
    void PollPoiDiscovery(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                          const RadarData::RadarConfig& cfg, const RadarData::TargetDatabase& db) {
        if (!cfg.ShowImportantPOI) return;
        if (!snap.LargeMap.IsVisible && !snap.MiniMap.IsVisible) return;
        const auto now = std::chrono::steady_clock::now();
        if (now - lastTgtPollTimePoint < std::chrono::milliseconds(5000)) return;
        lastTgtPollTimePoint = now;

        if (!RefreshTargetPatternCache(snap, db)) return;

        const int tgtCount = RadarRender::PoiDrawCache::CountMatchingTgtLocations(ctx,
                                                                                lastTargetPatterns);
        const int entCount = RadarRender::PoiDrawCache::CountMatchingEntities(snap,
                                                                              lastTargetPatterns);

        if (lastTgtMatchCount < 0 || lastEntityMatchCount < 0) {
            lastTgtMatchCount = tgtCount;
            lastEntityMatchCount = entCount;
            return;
        }
        if (tgtCount != lastTgtMatchCount || entCount != lastEntityMatchCount) {
            lastTgtMatchCount = tgtCount;
            lastEntityMatchCount = entCount;
            InvalidatePoi();
        }
    }

    void RebuildPoiIfNeeded(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                            const RadarData::RadarConfig& cfg,
                            const RadarData::TargetDatabase& db,
                            const RadarData::IconTables& icons) {
        if (!poiDirty) return;
        pois.Rebuild(ctx, snap, cfg, db, icons);
        poiDirty = false;
        if (cfg.ShowImportantPOI && RefreshTargetPatternCache(snap, db))
            RefreshPoiMatchCounts(ctx, snap);
        else {
            lastTgtMatchCount = -1;
            lastEntityMatchCount = -1;
        }
    }
};

} // namespace RadarPerf
