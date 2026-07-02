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
    struct PerfDebugState {
        bool captureThisFrame = false;
        bool walkableRebuildRan = false;
        bool poiRebuildRan = false;
        bool entityRebuildRan = false;
        double walkableRebuildMs = 0.0;
        double poiRebuildMs = 0.0;
        double entityRebuildMs = 0.0;

        void Reset(bool capture) {
            captureThisFrame = capture;
            walkableRebuildRan = false;
            poiRebuildRan = false;
            entityRebuildRan = false;
            walkableRebuildMs = 0.0;
            poiRebuildMs = 0.0;
            entityRebuildMs = 0.0;
        }
    };

    struct FrameSafetyState {
        bool heavyRebuildThisFrame = false;
        bool skipOptionalDetailThisFrame = false;
        bool skippedTerrainTextureBuildThisFrame = false;
        bool skippedBoundaryLinesThisFrame = false;
        bool skippedDotMatrixThisFrame = false;
        bool skippedPoiEdgeIndicatorsThisFrame = false;
        const char* optionalDetailSkipReason = nullptr;

        void Reset() {
            heavyRebuildThisFrame = false;
            skipOptionalDetailThisFrame = false;
            skippedTerrainTextureBuildThisFrame = false;
            skippedBoundaryLinesThisFrame = false;
            skippedDotMatrixThisFrame = false;
            skippedPoiEdgeIndicatorsThisFrame = false;
            optionalDetailSkipReason = nullptr;
        }

        void PromoteHeavy(const char* reason) {
            heavyRebuildThisFrame = true;
            skipOptionalDetailThisFrame = true;
            if (!optionalDetailSkipReason) optionalDetailSkipReason = reason;
        }
    };

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
    FrameSafetyState                frameSafety;
    PerfDebugState                  perfDebug;

    void BeginOverlayFrame(bool capturePerf = false) {
        frameSafety.Reset();
        perfDebug.Reset(capturePerf);
    }

    void MarkHeavyRebuildThisFrame() {
        frameSafety.PromoteHeavy("heavy rebuild");
    }

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
        frameSafety.Reset();
        perfDebug.Reset(false);
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
        const auto totalStart = std::chrono::steady_clock::now();
        MarkHeavyRebuildThisFrame();
        const bool areaChanged = snap.AreaChangeCounter != areaCounter;
        areaCounter = snap.AreaChangeCounter;
        walkablePtr = gridHandle.Data();
        const auto walkableStart = std::chrono::steady_clock::now();
        walkable.Rebuild(ctx, gridHandle, cfg);
        if (perfDebug.captureThisFrame) {
            perfDebug.walkableRebuildRan = true;
            perfDebug.walkableRebuildMs = std::chrono::duration<double, std::milli>(
                                           std::chrono::steady_clock::now() - walkableStart)
                                           .count();
        }
        const auto poiStart = std::chrono::steady_clock::now();
        pois.Rebuild(ctx, snap, cfg, db, icons);
        if (perfDebug.captureThisFrame) {
            perfDebug.poiRebuildRan = true;
            perfDebug.poiRebuildMs = std::chrono::duration<double, std::milli>(
                                      std::chrono::steady_clock::now() - poiStart)
                                      .count();
        }
        if (areaChanged) entities.Clear();
        const auto entityStart = std::chrono::steady_clock::now();
        entities.Rebuild(ctx, snap, cfg, db, icons);
        if (perfDebug.captureThisFrame) {
            perfDebug.entityRebuildRan = true;
            perfDebug.entityRebuildMs = std::chrono::duration<double, std::milli>(
                                         std::chrono::steady_clock::now() - entityStart)
                                         .count();
        }
        entitySnapshotTime = snap.LastUpdateTime;
        poiDirty = false;
        if (cfg.ShowImportantPOI) (void)RefreshTargetPatternCache(snap, db);
        lastTgtMatchCount = -1;
        lastEntityMatchCount = -1;
        (void)totalStart;
    }

    void RebuildEntitiesOnly(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                             const RadarData::RadarConfig& cfg,
                             const RadarData::TargetDatabase& db,
                            const RadarData::IconTables& icons) {
        const auto entityStart = std::chrono::steady_clock::now();
        entities.Rebuild(ctx, snap, cfg, db, icons);
        if (perfDebug.captureThisFrame) {
            perfDebug.entityRebuildRan = true;
            perfDebug.entityRebuildMs = std::chrono::duration<double, std::milli>(
                                         std::chrono::steady_clock::now() - entityStart)
                                         .count();
        }
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
        MarkHeavyRebuildThisFrame();
        const auto poiStart = std::chrono::steady_clock::now();
        pois.Rebuild(ctx, snap, cfg, db, icons);
        if (perfDebug.captureThisFrame) {
            perfDebug.poiRebuildRan = true;
            perfDebug.poiRebuildMs = std::chrono::duration<double, std::milli>(
                                      std::chrono::steady_clock::now() - poiStart)
                                      .count();
        }
        poiDirty = false;
        if (cfg.ShowImportantPOI) (void)RefreshTargetPatternCache(snap, db);
        lastTgtMatchCount = -1;
        lastEntityMatchCount = -1;
    }
};

} // namespace RadarPerf
