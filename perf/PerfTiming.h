#pragma once

#include "sdk/PluginSDK.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <sstream>
#include <string>

namespace RadarPerf {

class OverlayPerfTiming {
public:
    enum class Section : size_t {
        DrawUiTotal = 0,
        UiBlockerCheck,
        AtlasBlockerCheck = UiBlockerCheck,
        AreaCacheUpdate,
        TerrainBakeRebuild,
        TerrainTextureBuild,
        DotMatrixDraw,
        BoundaryLinesDraw,
        PoiCacheRebuild,
        PoiDraw,
        EntityCacheRebuild,
        EntityDraw,
        SettingsDebugUi,
        Count,
    };

    struct SectionStats {
        double lastMs = 0.0;
        double avgMs = 0.0;
        double maxMs = 0.0;
        size_t sampleCount = 0;
        bool   ranThisFrame = false;
    };

    struct FrameFlags {
        bool mapVisible = false;
        bool uiBlocked = false;
        bool atlasBlocked = false;
        std::string uiBlockerRule;
        std::string uiBlockerReason;
        bool uiBlockerRectValid = false;
        float uiBlockerRectX = 0.0f;
        float uiBlockerRectY = 0.0f;
        float uiBlockerRectW = 0.0f;
        float uiBlockerRectH = 0.0f;
        bool uiBlockerOverlapValid = false;
        float uiBlockerOverlapX = 0.0f;
        float uiBlockerOverlapY = 0.0f;
        float uiBlockerOverlapW = 0.0f;
        float uiBlockerOverlapH = 0.0f;
        float uiBlockerOverlapArea = 0.0f;
        int  uiBlockerMatchedRuleCount = 0;
        int  uiBlockerEvaluatedRuleCount = 0;
        std::string uiBlockerRejectedRulesSummary;
        bool heavyRebuildFrame = false;
        bool skippedOptionalDetail = false;
        const char* optionalDetailSkipReason = nullptr;
        bool debugToolsEnabled = false;
        bool perfTimingCaptureEnabled = false;
        bool uiDiscoveryEnabled = false;
        bool uiDiscoveryAutoRefreshEnabled = false;
        bool pinnedWatchEnabled = false;
        const char* dotRenderMode = nullptr;
        int  dotCandidatesTotal = 0;
        int  dotUserCellStep = 0;
        int  dotEffectiveCellStep = 0;
        bool dotAdaptiveSamplingActive = false;
        int  dotCandidatesDrawn = 0;
        int  dotMatrixDotsRasterized = 0;
        bool dotTextureRebuilt = false;
        double dotTextureBuildMs = 0.0;
        int  dotVectorDotsDrawn = 0;
        int  dotVectorDotsCulled = 0;
        int  dotCandidatesCulled = 0;
        int  dotCullProjectionFailed = 0;
        int  dotCullOutsideSurface = 0;
        int  dotCullBudget = 0;
        bool dotBudgetHit = false;
        const char* dotSkipReason = nullptr;
        const char* boundaryRenderMode = nullptr;
        const char* boundaryQualityMode = nullptr;
        const char* boundaryScopeMode = nullptr;
        int  boundaryTargetDrawCount = 0;
        int  boundarySegmentsTotal = 0;
        int  boundarySegmentsEligible = 0;
        int  boundarySegmentsDrawn = 0;
        int  boundarySegmentsRasterized = 0;
        int  boundarySegmentsCulled = 0;
        int  boundaryStride = 1;
        bool boundaryBudgetHit = false;
        bool boundaryTimeBudgetHit = false;
        double boundaryTimeBudgetMs = 0.0;
        bool boundaryTextureRebuilt = false;
        double boundaryTextureBuildMs = 0.0;
        const char* boundarySkipReason = nullptr;
        bool terrainTextureBuildCalled = false;
        int  terrainTextureWidth = 0;
        int  terrainTextureHeight = 0;
        int  terrainTexturePixelsWritten = 0;
        int  terrainTextureNonTransparentPixels = 0;
        bool terrainTextureUploadCalled = false;
        bool terrainTextureUploadSucceeded = false;
        bool terrainTextureDrawSubmitted = false;
        const char* terrainTextureDrawSkippedReason = nullptr;
        float terrainTextureDrawTintAlpha = 0.0f;
        bool terrainTextureDrawRectValid = false;
        float terrainTextureDrawRectMinX = 0.0f;
        float terrainTextureDrawRectMinY = 0.0f;
        float terrainTextureDrawRectMaxX = 0.0f;
        float terrainTextureDrawRectMaxY = 0.0f;
    };

    static constexpr size_t kSectionCount = static_cast<size_t>(Section::Count);
    static constexpr size_t kHistorySize = 180;

    bool captureEnabled = false;
    bool showPanel = false;
    bool freezeDisplay = false;
    FrameFlags flags{};

    void BeginFrame(bool enableCapture, const FrameFlags& frameFlags) {
        captureEnabled = enableCapture;
        flags = frameFlags;
        for (size_t i = 0; i < kSectionCount; ++i) {
            live_[i].ranThisFrame = false;
            live_[i].lastMs = 0.0;
        }
    }

    void Reset() {
        history_.fill({});
        historyIndex_ = 0;
        historyCount_ = 0;
        live_.fill({});
        frozen_ = live_;
    }

    void Record(Section section, double ms, bool ran = true) {
        if (!captureEnabled) return;
        live_[static_cast<size_t>(section)].lastMs = ms;
        live_[static_cast<size_t>(section)].ranThisFrame = ran;
        history_[historyIndex_][static_cast<size_t>(section)] = ms;
        live_[static_cast<size_t>(section)].sampleCount = std::min(historyCount_ + 1, kHistorySize);
    }

    void RecordExternal(Section section, double ms, bool enableCapture, bool ran = true) {
        if (!enableCapture) return;
        const bool oldCapture = captureEnabled;
        captureEnabled = true;
        Record(section, ms, ran);
        captureEnabled = oldCapture;
    }

    void MarkSkipped(Section section) {
        if (!captureEnabled) return;
        live_[static_cast<size_t>(section)].lastMs = 0.0;
        live_[static_cast<size_t>(section)].ranThisFrame = false;
    }

    void EndFrame() {
        if (!captureEnabled) {
            if (!freezeDisplay) frozen_ = live_;
            return;
        }

        historyIndex_ = (historyIndex_ + 1) % kHistorySize;
        historyCount_ = std::min(historyCount_ + 1, kHistorySize);

        for (size_t section = 0; section < kSectionCount; ++section) {
            double sum = 0.0;
            double max = 0.0;
            for (size_t i = 0; i < historyCount_; ++i) {
                const double sample = history_[i][section];
                sum += sample;
                max = std::max(max, sample);
            }
            live_[section].avgMs = historyCount_ ? sum / static_cast<double>(historyCount_) : 0.0;
            live_[section].maxMs = max;
            live_[section].sampleCount = historyCount_;
        }

        if (!freezeDisplay) frozen_ = live_;
    }

    const SectionStats& Stats(Section section) const {
        return freezeDisplay ? frozen_[static_cast<size_t>(section)] : live_[static_cast<size_t>(section)];
    }

    static const char* Name(Section section) {
        switch (section) {
            case Section::DrawUiTotal: return "DrawUI total";
            case Section::UiBlockerCheck: return "UI blocker check";
            case Section::AreaCacheUpdate: return "Area cache update";
            case Section::TerrainBakeRebuild: return "Terrain bake/rebuild";
            case Section::TerrainTextureBuild: return "Terrain texture build/update";
            case Section::DotMatrixDraw: return "Dot matrix draw";
            case Section::BoundaryLinesDraw: return "Boundary lines draw";
            case Section::PoiCacheRebuild: return "POI cache rebuild";
            case Section::PoiDraw: return "POI draw";
            case Section::EntityCacheRebuild: return "Entity cache rebuild";
            case Section::EntityDraw: return "Entity draw";
            case Section::SettingsDebugUi: return "Settings/debug UI";
            default: return "Unknown";
        }
    }

    std::string BuildLastFrameReport() const {
        std::ostringstream out;
        for (size_t i = 0; i < kSectionCount; ++i) {
            const auto section = static_cast<Section>(i);
            const auto& stats = Stats(section);
            out << Name(section) << ": " << stats.lastMs << " ms | ran="
                << (stats.ranThisFrame ? "yes" : "no") << "\n";
        }
        return out.str();
    }

    std::string BuildRollingSummary() const {
        std::ostringstream out;
        for (size_t i = 0; i < kSectionCount; ++i) {
            const auto section = static_cast<Section>(i);
            const auto& stats = Stats(section);
            out << Name(section) << " | last=" << stats.lastMs << " avg=" << stats.avgMs
                << " max=" << stats.maxMs << " samples=" << stats.sampleCount << "\n";
        }
        return out.str();
    }

    std::string BuildFullReport(const PluginSDK::Snapshot& snap) const {
        std::ostringstream out;
        out << "Alt Radar Perf Report\n";
        out << "Samples: " << Stats(Section::DrawUiTotal).sampleCount << "\n";
        out << "Area: " << snap.CurrentAreaName << "\n";
        out << "AreaHash: " << snap.CurrentAreaHash << "\n";
        out << "MapVisible: " << (flags.mapVisible ? "yes" : "no") << "\n";
        out << "UiBlocked: " << (flags.uiBlocked ? "yes" : "no") << "\n";
        out << "AtlasBlocked: " << (flags.atlasBlocked ? "yes" : "no") << "\n";
        out << "UiBlockerRule: " << (flags.uiBlockerRule.empty() ? "(none)" : flags.uiBlockerRule) << "\n";
        out << "UiBlockerReason: " << (flags.uiBlockerReason.empty() ? "(none)" : flags.uiBlockerReason) << "\n";
        if (flags.uiBlockerRectValid) {
            out << "UiBlockerRect: [" << flags.uiBlockerRectX << ", " << flags.uiBlockerRectY
                << "] " << flags.uiBlockerRectW << "x" << flags.uiBlockerRectH << "\n";
        } else {
            out << "UiBlockerRect: (none)\n";
        }
        if (flags.uiBlockerOverlapValid) {
            out << "UiBlockerOverlap: [" << flags.uiBlockerOverlapX << ", "
                << flags.uiBlockerOverlapY << "] " << flags.uiBlockerOverlapW << "x"
                << flags.uiBlockerOverlapH << " area=" << flags.uiBlockerOverlapArea << "\n";
        } else {
            out << "UiBlockerOverlap: (none)\n";
        }
        out << "UiBlockerMatchedRuleCount: " << flags.uiBlockerMatchedRuleCount << "\n";
        out << "UiBlockerEvaluatedRuleCount: " << flags.uiBlockerEvaluatedRuleCount << "\n";
        out << "UiBlockerRejectedRules: "
            << (flags.uiBlockerRejectedRulesSummary.empty() ? "(none)"
                                                            : flags.uiBlockerRejectedRulesSummary)
            << "\n";
        out << "HeavyRebuildFrame: " << (flags.heavyRebuildFrame ? "yes" : "no") << "\n";
        out << "SkippedOptionalDetail: " << (flags.skippedOptionalDetail ? "yes" : "no") << "\n";
        out << "OptionalDetailSkipReason: "
            << (flags.optionalDetailSkipReason ? flags.optionalDetailSkipReason : "(none)") << "\n";
        out << "DebugToolsEnabled: " << (flags.debugToolsEnabled ? "yes" : "no") << "\n";
        out << "PerfTimingCaptureEnabled: " << (flags.perfTimingCaptureEnabled ? "yes" : "no") << "\n";
        out << "UiDiscoveryEnabled: " << (flags.uiDiscoveryEnabled ? "yes" : "no") << "\n";
        out << "UiDiscoveryAutoRefreshEnabled: "
            << (flags.uiDiscoveryAutoRefreshEnabled ? "yes" : "no") << "\n";
        out << "PinnedWatchEnabled: " << (flags.pinnedWatchEnabled ? "yes" : "no") << "\n\n";
        out << "DotMatrixRenderMode: " << (flags.dotRenderMode ? flags.dotRenderMode : "Disabled") << "\n";
        out << "DotCandidatesTotal: " << flags.dotCandidatesTotal << "\n";
        out << "DotUserCellStep: " << flags.dotUserCellStep << "\n";
        out << "DotEffectiveCellStep: " << flags.dotEffectiveCellStep << "\n";
        out << "DotAdaptiveSamplingActive: " << (flags.dotAdaptiveSamplingActive ? "yes" : "no") << "\n";
        out << "DotCandidatesDrawn: " << flags.dotCandidatesDrawn << "\n";
        out << "DotMatrixDotsRasterized: " << flags.dotMatrixDotsRasterized << "\n";
        out << "DotMatrixTextureRebuilt: " << (flags.dotTextureRebuilt ? "yes" : "no") << "\n";
        out << "DotMatrixTextureBuildMs: " << flags.dotTextureBuildMs << "\n";
        out << "DotMatrixVectorDotsDrawn: " << flags.dotVectorDotsDrawn << "\n";
        out << "DotMatrixVectorDotsCulled: " << flags.dotVectorDotsCulled << "\n";
        out << "DotCandidatesCulled: " << flags.dotCandidatesCulled << "\n";
        out << "DotCullProjectionFailed: " << flags.dotCullProjectionFailed << "\n";
        out << "DotCullOutsideSurface: " << flags.dotCullOutsideSurface << "\n";
        out << "DotCullBudget: " << flags.dotCullBudget << "\n";
        out << "DotBudgetHit: " << (flags.dotBudgetHit ? "yes" : "no") << "\n";
        out << "DotSkipReason: " << (flags.dotSkipReason ? flags.dotSkipReason : "(none)") << "\n";
        out << "BoundaryRenderMode: "
            << (flags.boundaryRenderMode ? flags.boundaryRenderMode : "(unknown)") << "\n";
        out << "BoundaryQualityMode: "
            << (flags.boundaryQualityMode ? flags.boundaryQualityMode : "(unknown)") << "\n";
        out << "BoundaryScopeMode: "
            << (flags.boundaryScopeMode ? flags.boundaryScopeMode : "(unknown)") << "\n";
        out << "BoundaryTargetDrawCount: " << flags.boundaryTargetDrawCount << "\n";
        out << "BoundarySegmentsTotal: " << flags.boundarySegmentsTotal << "\n";
        out << "BoundarySegmentsEligible: " << flags.boundarySegmentsEligible << "\n";
        out << "BoundarySegmentsDrawn: " << flags.boundarySegmentsDrawn << "\n";
        out << "BoundarySegmentsRasterized: " << flags.boundarySegmentsRasterized << "\n";
        out << "BoundarySegmentsCulled: " << flags.boundarySegmentsCulled << "\n";
        out << "BoundaryStride: " << flags.boundaryStride << "\n";
        out << "BoundaryBudgetHit: " << (flags.boundaryBudgetHit ? "yes" : "no") << "\n\n";
        out << "BoundaryTimeBudgetHit: " << (flags.boundaryTimeBudgetHit ? "yes" : "no") << "\n";
        out << "BoundaryTimeBudgetMs: " << flags.boundaryTimeBudgetMs << "\n";
        out << "BoundaryTextureRebuilt: " << (flags.boundaryTextureRebuilt ? "yes" : "no") << "\n";
        out << "BoundaryTextureBuildMs: " << flags.boundaryTextureBuildMs << "\n";
        out << "BoundarySkipReason: " << (flags.boundarySkipReason ? flags.boundarySkipReason : "(none)") << "\n\n";
        out << "TerrainTextureBuildCalled: " << (flags.terrainTextureBuildCalled ? "yes" : "no") << "\n";
        out << "TerrainTextureWidth: " << flags.terrainTextureWidth << "\n";
        out << "TerrainTextureHeight: " << flags.terrainTextureHeight << "\n";
        out << "TerrainTexturePixelsWritten: " << flags.terrainTexturePixelsWritten << "\n";
        out << "TerrainTextureNonTransparentPixels: " << flags.terrainTextureNonTransparentPixels << "\n";
        out << "TerrainTextureUploadCalled: " << (flags.terrainTextureUploadCalled ? "yes" : "no") << "\n";
        out << "TerrainTextureUploadSucceeded: " << (flags.terrainTextureUploadSucceeded ? "yes" : "no") << "\n";
        out << "TerrainTextureDrawSubmitted: " << (flags.terrainTextureDrawSubmitted ? "yes" : "no") << "\n";
        out << "TerrainTextureDrawSkippedReason: "
            << (flags.terrainTextureDrawSkippedReason ? flags.terrainTextureDrawSkippedReason : "(none)") << "\n";
        out << "TerrainTextureDrawTintAlpha: " << flags.terrainTextureDrawTintAlpha << "\n";
        if (flags.terrainTextureDrawRectValid) {
            out << "TerrainTextureDrawRect: [" << flags.terrainTextureDrawRectMinX << ", "
                << flags.terrainTextureDrawRectMinY << "] -> [" << flags.terrainTextureDrawRectMaxX
                << ", " << flags.terrainTextureDrawRectMaxY << "]\n\n";
        } else {
            out << "TerrainTextureDrawRect: (none)\n\n";
        }
        out << BuildRollingSummary();
        return out.str();
    }

private:
    std::array<std::array<double, kSectionCount>, kHistorySize> history_{};
    size_t historyIndex_ = 0;
    size_t historyCount_ = 0;
    std::array<SectionStats, kSectionCount> live_{};
    std::array<SectionStats, kSectionCount> frozen_{};
};

} // namespace RadarPerf
