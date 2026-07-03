#pragma once

#include "RadarLog.h"
#include "RadarTypes.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>

#include "../third_party/json.hpp"

namespace RadarData {

enum class TerrainTextureAlignmentMode : int {
    Legacy = 0,
    CellCentered = 1,
    ZeroBased = 2,
};

inline const char* TerrainTextureAlignmentModeName(TerrainTextureAlignmentMode mode) {
    switch (mode) {
        case TerrainTextureAlignmentMode::CellCentered:
            return "Cell-centred";
        case TerrainTextureAlignmentMode::ZeroBased:
            return "Zero-based";
        default:
            return "Current / Legacy";
    }
}

enum class TerrainProjectionHeightMode : int {
    Legacy = 0,
    Flat = 1,
    RelativeToPlayer = 2,
    FlatPlayerAnchored = 3,
};

inline const char* TerrainProjectionHeightModeName(TerrainProjectionHeightMode mode) {
    switch (mode) {
        case TerrainProjectionHeightMode::Flat:
            return "Flat / Ignore Z";
        case TerrainProjectionHeightMode::RelativeToPlayer:
            return "Relative to Player Z";
        case TerrainProjectionHeightMode::FlatPlayerAnchored:
            return "Flat / Player Anchored";
        default:
            return "Terrain Height / Legacy";
    }
}

enum class TerrainProjectionMode : int {
    Normal = 0,
    SdkOnly = 1,
    FallbackOnly = 2,
};

inline const char* TerrainProjectionModeName(TerrainProjectionMode mode) {
    switch (mode) {
        case TerrainProjectionMode::SdkOnly:
            return "SDK Only";
        case TerrainProjectionMode::FallbackOnly:
            return "Fallback Only";
        default:
            return "Normal";
    }
}

enum class TerrainRenderStyle : int {
    Texture = 0,
    DotMatrix = 1,
    TextureAndDotMatrix = 2,
};

inline const char* TerrainRenderStyleName(TerrainRenderStyle style) {
    switch (style) {
        case TerrainRenderStyle::DotMatrix:
            return "Dot Matrix";
        case TerrainRenderStyle::TextureAndDotMatrix:
            return "Texture + Dot Matrix";
        default:
            return "Texture";
    }
}

enum class TerrainBoundaryQualityMode : int {
    Off = 0,
    Low = 1,
    Medium = 2,
    High = 3,
    Unlimited = 4,
};

inline const char* TerrainBoundaryQualityModeName(TerrainBoundaryQualityMode mode) {
    switch (mode) {
        case TerrainBoundaryQualityMode::Off: return "Off";
        case TerrainBoundaryQualityMode::Low: return "Low";
        case TerrainBoundaryQualityMode::Medium: return "Medium";
        case TerrainBoundaryQualityMode::High: return "High";
        case TerrainBoundaryQualityMode::Unlimited: return "Unlimited / Debug";
        default: return "Medium";
    }
}

enum class TerrainBoundaryRenderMode : int {
    CachedTexture = 0,
    VectorLinesDebug = 1,
    Off = 2,
};

inline const char* TerrainBoundaryRenderModeName(TerrainBoundaryRenderMode mode) {
    switch (mode) {
        case TerrainBoundaryRenderMode::VectorLinesDebug: return "Vector Lines / Debug";
        case TerrainBoundaryRenderMode::Off: return "Off";
        default: return "Cached Texture / Rasterized Edges";
    }
}

enum class DotMatrixRenderMode : int {
    CachedTexture = 0,
    VectorDotsDebug = 1,
};

inline const char* DotMatrixRenderModeName(DotMatrixRenderMode mode) {
    switch (mode) {
        case DotMatrixRenderMode::VectorDotsDebug: return "Vector Dots / Debug";
        default: return "Cached Texture / Rasterized Dots";
    }
}

inline DotMatrixRenderMode EffectiveDotMatrixRenderMode(bool debugToolsEnabled,
                                                        DotMatrixRenderMode configuredMode) {
    return debugToolsEnabled ? configuredMode : DotMatrixRenderMode::CachedTexture;
}

inline int SanitizeBoundaryEdgeThickness(int thickness) {
    return std::clamp(thickness, 1, 3);
}

enum class TerrainBoundaryScopeMode : int {
    VisibleMapBudgeted = 0,
    NearPlayerOnly = 1,
};

inline const char* TerrainBoundaryScopeModeName(TerrainBoundaryScopeMode mode) {
    switch (mode) {
        case TerrainBoundaryScopeMode::NearPlayerOnly: return "Near Player Only";
        default: return "Visible Map / Budgeted";
    }
}

enum class MapLayerProjectionMode : int {
    NativeSdk = 0,
    Unified2D = 1,
};

inline const char* MapLayerProjectionModeName(MapLayerProjectionMode mode) {
    switch (mode) {
        case MapLayerProjectionMode::Unified2D:
            return "Unified 2D / POE2GPS-style";
        default:
            return "Native / SDK";
    }
}

inline Rgba8 ParseRgbString(const std::string& s, Rgba8 fallback = {}) {
    int r = fallback.r, g = fallback.g, b = fallback.b;
    if (sscanf_s(s.c_str(), "%d, %d, %d", &r, &g, &b) >= 3
        || sscanf_s(s.c_str(), "%d,%d,%d", &r, &g, &b) >= 3)
        return Rgba8{static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                     static_cast<uint8_t>(b), fallback.a};
    return fallback;
}

struct RadarConfig {
    static constexpr int kRuneShapeRareCount = 11;
    static constexpr int kRuneShapeCommonCount = 23;

    bool  OverlayEnabled = true;
    bool  EnableDebugTools = false;
    bool  EnablePerformanceDebug = false;
    bool  EnablePerfTimingCapture = false;
    bool  ShowPerfTimingPanel = false;
    bool  EnableUiDiscoveryDebug = false;
    bool  EnableUiDiscoveryTools = false;
    bool  EnableUiDiscoveryAutoRefresh = false;
    bool  EnablePinnedUiWatch = false;
    bool  EnableUiBlockerDebug = false;
    bool  ShowUiBlockerDebugDetails = false;
    bool  EnableDrawDebug = false;
    bool  ShowTerrainTextureDebug = false;
    bool  ShowDotMatrixDebug = false;
    bool  ShowBoundaryDebug = false;
    bool  DrawWhenNotInHideoutOrTown = true;
    bool  DrawWhenNotPaused = true;
    bool  HideWhenNotForeground = true;
    bool  EnableUiBlockerDetection = true;
    bool  HideOutsideNetworkBubble = false;
    bool  DrawWalkableMap = true;
    bool  DrawMiniMapEntities = true;
    bool  ShowBoundaryEdges = false;
    int   WalkableMapBorderThickness = 1;
    DotMatrixRenderMode       DotRenderMode = DotMatrixRenderMode::CachedTexture;
    TerrainBoundaryRenderMode  BoundaryRenderMode = TerrainBoundaryRenderMode::CachedTexture;
    TerrainBoundaryQualityMode BoundaryQuality = TerrainBoundaryQualityMode::Medium;
    TerrainBoundaryScopeMode   BoundaryScope = TerrainBoundaryScopeMode::VisibleMapBudgeted;
    float BoundaryNearPlayerRadius = 500.f;
    TerrainRenderStyle TerrainStyle = TerrainRenderStyle::Texture;
    TerrainTextureAlignmentMode TerrainAlignment = TerrainTextureAlignmentMode::CellCentered;
    TerrainProjectionHeightMode TerrainHeightMode = TerrainProjectionHeightMode::Legacy;
    TerrainProjectionMode TerrainProjection = TerrainProjectionMode::Normal;
    MapLayerProjectionMode MapProjectionMode = MapLayerProjectionMode::Unified2D;
    int   DotCellStep = 2;
    float DotSize = 1.5f;
    bool  ShowPlayerNames = false;
    bool  ShowImportantPOI = true;
    bool  DrawPoiIcons = false;
    bool  EnablePOIBackground = true;
    bool  RuneShapeShowWeights = false;
    int   RuneShapeMinimumWeight = 0;
    std::array<int, kRuneShapeRareCount> RuneShapeRareWeights{};
    std::array<int, kRuneShapeCommonCount> RuneShapeCommonWeights{};
    bool  EdgeIndicatorMinimap = true;
    bool  EdgeIndicatorLargemap = true;
    bool  UseLegacyClassifier = false;
    float LargeMapScaleMultiplier = 0.1738f;
    ImVec4 TextureInteriorColor{0.46f, 0.46f, 0.46f, 0.7f};
    ImVec4 TextureWallEdgeColor{60.0f / 255.0f, 220.0f / 255.0f, 1.0f, 180.0f / 255.0f};
    ImVec4 DotMatrixFillColor{0.46f, 0.46f, 0.46f, 0.7f};
    ImVec4 POIColor{1.f, 1.f, 0.5f, 1.f};
    int   MaxEntitiesDrawn = 512;
    ImVec2 MainMenuSize{900.f, 600.f};

    void Load(const std::filesystem::path& pluginDir) {
        const auto path = pluginDir / "config" / "settings.json";
        if (!std::filesystem::exists(path)) return;
        std::ifstream in(path);
        if (!in.is_open()) return;
        nlohmann::json j;
        try {
            in >> j;
        } catch (const std::exception& ex) {
            RadarLog::Instance().Warn(std::string("settings.json parse failed; using defaults: ") + ex.what());
            return;
        } catch (...) {
            RadarLog::Instance().Warn("settings.json parse failed; using defaults");
            return;
        }
        auto readColor = [&](const char* key, ImVec4& out) {
            if (!j.contains(key) || !j[key].is_array() || j[key].size() < 4) return false;
            auto& a = j[key];
            out = ImVec4(a[0].get<float>(), a[1].get<float>(), a[2].get<float>(),
                         a[3].get<float>());
            return true;
        };

        try {
        OverlayEnabled = j.value("OverlayEnabled", OverlayEnabled);
        EnableDebugTools = j.value("EnableDebugTools", EnableDebugTools);
        EnablePerfTimingCapture = j.value("EnablePerfTimingCapture", EnablePerfTimingCapture);
        ShowPerfTimingPanel = j.value("ShowPerfTimingPanel", ShowPerfTimingPanel);
        EnableUiDiscoveryTools = j.value("EnableUiDiscoveryTools", EnableUiDiscoveryTools);
        EnableUiDiscoveryAutoRefresh = j.value("EnableUiDiscoveryAutoRefresh", EnableUiDiscoveryAutoRefresh);
        EnablePinnedUiWatch = j.value("EnablePinnedUiWatch", EnablePinnedUiWatch);
        ShowUiBlockerDebugDetails = j.value("ShowUiBlockerDebugDetails",
                                            j.value("ShowAtlasBlockerDebug",
                                                    ShowUiBlockerDebugDetails));
        ShowTerrainTextureDebug = j.value("ShowTerrainTextureDebug", ShowTerrainTextureDebug);
        ShowDotMatrixDebug = j.value("ShowDotMatrixDebug", ShowDotMatrixDebug);
        ShowBoundaryDebug = j.value("ShowBoundaryDebug", ShowBoundaryDebug);
        const bool legacyPerformanceDebug = ShowPerfTimingPanel || EnablePerfTimingCapture;
        const bool legacyUiDiscoveryDebug = EnableUiDiscoveryTools || EnableUiDiscoveryAutoRefresh
                                            || EnablePinnedUiWatch;
        const bool legacyUiBlockerDebug = ShowUiBlockerDebugDetails;
        const bool legacyDrawDebug = ShowTerrainTextureDebug || ShowDotMatrixDebug || ShowBoundaryDebug;
        EnablePerformanceDebug = j.value("EnablePerformanceDebug", legacyPerformanceDebug);
        EnableUiDiscoveryDebug = j.value("EnableUiDiscoveryDebug", legacyUiDiscoveryDebug);
        EnableUiBlockerDebug = j.value("EnableUiBlockerDebug", legacyUiBlockerDebug);
        EnableDrawDebug = j.value("EnableDrawDebug", legacyDrawDebug);
        DrawWhenNotInHideoutOrTown = j.value("DrawWhenNotInHideoutOrTown", DrawWhenNotInHideoutOrTown);
        DrawWhenNotPaused = j.value("DrawWhenNotPaused", DrawWhenNotPaused);
        HideWhenNotForeground = j.value("HideWhenNotForeground", HideWhenNotForeground);
        EnableUiBlockerDetection = j.value("EnableUiBlockerDetection", EnableUiBlockerDetection);
        HideOutsideNetworkBubble = j.value("HideOutsideNetworkBubble", HideOutsideNetworkBubble);
        DrawWalkableMap = j.value("DrawWalkableMap", DrawWalkableMap);
        // Legacy minimap terrain experiments were removed from runtime, but keep the old
        // key accepted so existing settings files continue to load cleanly.
        (void)j.value("DrawMiniMapTerrain", false);
        DrawMiniMapEntities = j.value("DrawMiniMapEntities", DrawMiniMapEntities);
        const int loadedBoundaryThickness =
            j.value("WalkableMapBorderThickness", WalkableMapBorderThickness);
        DotRenderMode = static_cast<DotMatrixRenderMode>(
            std::clamp(j.value("DotRenderMode", static_cast<int>(DotRenderMode)), 0, 1));
        BoundaryRenderMode = static_cast<TerrainBoundaryRenderMode>(
            std::clamp(j.value("BoundaryRenderMode", static_cast<int>(BoundaryRenderMode)), 0, 2));
        BoundaryQuality = static_cast<TerrainBoundaryQualityMode>(
            std::clamp(j.value("BoundaryQuality", static_cast<int>(BoundaryQuality)), 0, 4));
        BoundaryScope = static_cast<TerrainBoundaryScopeMode>(
            std::clamp(j.value("BoundaryScope", static_cast<int>(BoundaryScope)), 0, 1));
        BoundaryNearPlayerRadius = std::clamp(j.value("BoundaryNearPlayerRadius", BoundaryNearPlayerRadius),
                                              50.f, 5000.f);
        const bool legacyBoundaryEnabled = loadedBoundaryThickness > 0
                                           && BoundaryRenderMode != TerrainBoundaryRenderMode::Off;
        ShowBoundaryEdges = j.value("ShowBoundaryEdges", legacyBoundaryEnabled);
        WalkableMapBorderThickness = SanitizeBoundaryEdgeThickness(loadedBoundaryThickness);
        TerrainStyle = static_cast<TerrainRenderStyle>(
            std::clamp(j.value("TerrainStyle", static_cast<int>(TerrainStyle)), 0, 2));
        // Terrain alignment experiments are retired; keep loading the old key but normalize
        // runtime behavior to the working cell-centred mode.
        (void)j.value("TerrainAlignment", static_cast<int>(TerrainAlignment));
        TerrainAlignment = TerrainTextureAlignmentMode::CellCentered;
        TerrainHeightMode = static_cast<TerrainProjectionHeightMode>(
            std::clamp(j.value("TerrainHeightMode", static_cast<int>(TerrainHeightMode)), 0, 3));
        TerrainProjection = static_cast<TerrainProjectionMode>(
            std::clamp(j.value("TerrainProjection", static_cast<int>(TerrainProjection)), 0, 2));
        // Unified 2D is the only supported large-map projection mode now.
        (void)j.value("MapLayerProjectionMode", static_cast<int>(MapProjectionMode));
        MapProjectionMode = MapLayerProjectionMode::Unified2D;
        DotCellStep = std::clamp(j.value("DotCellStep", j.value("WalkableDecimation", DotCellStep)), 1, 16);
        DotSize = std::clamp(j.value("DotSize", DotSize), 0.5f, 6.0f);
        ShowPlayerNames = j.value("ShowPlayerNames", ShowPlayerNames);
        ShowImportantPOI = j.value("ShowImportantPOI", ShowImportantPOI);
        DrawPoiIcons = j.value("DrawPoiIcons", DrawPoiIcons);
        EnablePOIBackground = j.value("EnablePOIBackground", EnablePOIBackground);
        RuneShapeShowWeights = j.value("RuneShapeShowWeights", RuneShapeShowWeights);
        (void)j.value("RuneShapeShowPath", false);
        RuneShapeMinimumWeight = std::clamp(j.value("RuneShapeMinimumWeight", RuneShapeMinimumWeight), -100, 100);
        auto readIntArray = [&](const char* key, auto& out) {
            if (!j.contains(key) || !j[key].is_array()) return;
            const auto& a = j[key];
            for (size_t i = 0; i < out.size() && i < a.size(); ++i)
                if (a[i].is_number_integer()) out[i] = std::clamp(a[i].get<int>(), -100, 100);
        };
        readIntArray("RuneShapeRareWeights", RuneShapeRareWeights);
        readIntArray("RuneShapeCommonWeights", RuneShapeCommonWeights);
        EdgeIndicatorMinimap = j.value("EdgeIndicatorMinimap", EdgeIndicatorMinimap);
        EdgeIndicatorLargemap = j.value("EdgeIndicatorLargemap", EdgeIndicatorLargemap);
        UseLegacyClassifier = j.value("UseLegacyClassifier", UseLegacyClassifier);
        LargeMapScaleMultiplier = j.value("LargeMapScaleMultiplier", LargeMapScaleMultiplier);
        MaxEntitiesDrawn = std::clamp(j.value("MaxEntitiesDrawn", MaxEntitiesDrawn), 64, 4096);
        const bool hasTextureInterior = readColor("TextureInteriorColor", TextureInteriorColor);
        if (!hasTextureInterior) {
            const bool hasLegacyInterior = readColor("WalkableMapInteriorColor", TextureInteriorColor);
            if (!hasLegacyInterior)
                readColor("WalkableMapColor", TextureInteriorColor);
        }
        if (!readColor("TextureWallEdgeColor", TextureWallEdgeColor))
            readColor("WalkableMapEdgeColor", TextureWallEdgeColor);
        if (!readColor("DotMatrixFillColor", DotMatrixFillColor))
            DotMatrixFillColor = TextureInteriorColor;
        readColor("POIColor", POIColor);
        if (j.contains("MainMenuSize") && j["MainMenuSize"].is_array()
            && j["MainMenuSize"].size() >= 2) {
            MainMenuSize.x = j["MainMenuSize"][0].get<float>();
            MainMenuSize.y = j["MainMenuSize"][1].get<float>();
        }
        } catch (const std::exception& ex) {
            RadarLog::Instance().Warn(std::string("settings.json invalid; using loaded defaults where possible: ") + ex.what());
        } catch (...) {
            RadarLog::Instance().Warn("settings.json invalid; using loaded defaults where possible");
        }
    }

    void Save(const std::filesystem::path& pluginDir) const {
        const auto path = pluginDir / "config" / "settings.json";
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        nlohmann::json j;
        j["OverlayEnabled"] = OverlayEnabled;
        j["EnableDebugTools"] = EnableDebugTools;
        j["EnablePerformanceDebug"] = EnablePerformanceDebug;
        j["EnablePerfTimingCapture"] = EnablePerfTimingCapture;
        j["ShowPerfTimingPanel"] = ShowPerfTimingPanel;
        j["EnableUiDiscoveryDebug"] = EnableUiDiscoveryDebug;
        j["EnableUiDiscoveryTools"] = EnableUiDiscoveryTools;
        j["EnableUiDiscoveryAutoRefresh"] = EnableUiDiscoveryAutoRefresh;
        j["EnablePinnedUiWatch"] = EnablePinnedUiWatch;
        j["EnableUiBlockerDebug"] = EnableUiBlockerDebug;
        j["ShowUiBlockerDebugDetails"] = ShowUiBlockerDebugDetails;
        j["ShowAtlasBlockerDebug"] = ShowUiBlockerDebugDetails;
        j["EnableDrawDebug"] = EnableDrawDebug;
        j["ShowTerrainTextureDebug"] = ShowTerrainTextureDebug;
        j["ShowDotMatrixDebug"] = ShowDotMatrixDebug;
        j["ShowBoundaryDebug"] = ShowBoundaryDebug;
        j["DrawWhenNotInHideoutOrTown"] = DrawWhenNotInHideoutOrTown;
        j["DrawWhenNotPaused"] = DrawWhenNotPaused;
        j["HideWhenNotForeground"] = HideWhenNotForeground;
        j["EnableUiBlockerDetection"] = EnableUiBlockerDetection;
        j["HideOutsideNetworkBubble"] = HideOutsideNetworkBubble;
        j["DrawWalkableMap"] = DrawWalkableMap;
        j["DrawMiniMapEntities"] = DrawMiniMapEntities;
        j["ShowBoundaryEdges"] = ShowBoundaryEdges;
        j["WalkableMapBorderThickness"] = SanitizeBoundaryEdgeThickness(WalkableMapBorderThickness);
        j["DotRenderMode"] = static_cast<int>(DotRenderMode);
        j["BoundaryRenderMode"] = static_cast<int>(BoundaryRenderMode);
        j["BoundaryQuality"] = static_cast<int>(BoundaryQuality);
        j["BoundaryScope"] = static_cast<int>(BoundaryScope);
        j["BoundaryNearPlayerRadius"] = BoundaryNearPlayerRadius;
        j["TerrainStyle"] = static_cast<int>(TerrainStyle);
        j["TerrainHeightMode"] = static_cast<int>(TerrainHeightMode);
        j["DotCellStep"] = DotCellStep;
        j["DotSize"] = DotSize;
        j["WalkableDecimation"] = DotCellStep;
        j["ShowImportantPOI"] = ShowImportantPOI;
        j["DrawPoiIcons"] = DrawPoiIcons;
        j["EnablePOIBackground"] = EnablePOIBackground;
        j["RuneShapeShowWeights"] = RuneShapeShowWeights;
        j["RuneShapeMinimumWeight"] = RuneShapeMinimumWeight;
        j["RuneShapeRareWeights"] = RuneShapeRareWeights;
        j["RuneShapeCommonWeights"] = RuneShapeCommonWeights;
        j["EdgeIndicatorMinimap"] = EdgeIndicatorMinimap;
        j["EdgeIndicatorLargemap"] = EdgeIndicatorLargemap;
        j["UseLegacyClassifier"] = UseLegacyClassifier;
        j["MaxEntitiesDrawn"] = MaxEntitiesDrawn;
        j["TextureInteriorColor"] = {TextureInteriorColor.x, TextureInteriorColor.y,
                                      TextureInteriorColor.z, TextureInteriorColor.w};
        j["TextureWallEdgeColor"] = {TextureWallEdgeColor.x, TextureWallEdgeColor.y,
                                      TextureWallEdgeColor.z, TextureWallEdgeColor.w};
        j["DotMatrixFillColor"] = {DotMatrixFillColor.x, DotMatrixFillColor.y,
                                    DotMatrixFillColor.z, DotMatrixFillColor.w};
        j["WalkableMapInteriorColor"] = {TextureInteriorColor.x, TextureInteriorColor.y,
                                          TextureInteriorColor.z, TextureInteriorColor.w};
        j["WalkableMapEdgeColor"] = {TextureWallEdgeColor.x, TextureWallEdgeColor.y,
                                      TextureWallEdgeColor.z, TextureWallEdgeColor.w};
        j["WalkableMapColor"] = {TextureInteriorColor.x, TextureInteriorColor.y,
                                  TextureInteriorColor.z, TextureInteriorColor.w};
        j["POIColor"] = {POIColor.x, POIColor.y, POIColor.z, POIColor.w};
        std::ofstream out(path);
        if (out.is_open()) out << j.dump(4);
    }
};

} // namespace RadarData
