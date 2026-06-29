#pragma once

#include "RadarTypes.h"

#include <algorithm>
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
    bool  OverlayEnabled = true;
    bool  DrawWhenNotInHideoutOrTown = true;
    bool  DrawWhenNotPaused = true;
    bool  HideWhenNotForeground = true;
    bool  HideOutsideNetworkBubble = false;
    bool  DrawWalkableMap = true;
    bool  DrawMiniMapEntities = true;
    int   WalkableMapBorderThickness = 0;
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
        in >> j;
        auto readColor = [&](const char* key, ImVec4& out) {
            if (!j.contains(key) || !j[key].is_array() || j[key].size() < 4) return false;
            auto& a = j[key];
            out = ImVec4(a[0].get<float>(), a[1].get<float>(), a[2].get<float>(),
                         a[3].get<float>());
            return true;
        };

        OverlayEnabled = j.value("OverlayEnabled", OverlayEnabled);
        DrawWhenNotInHideoutOrTown = j.value("DrawWhenNotInHideoutOrTown", DrawWhenNotInHideoutOrTown);
        DrawWhenNotPaused = j.value("DrawWhenNotPaused", DrawWhenNotPaused);
        HideWhenNotForeground = j.value("HideWhenNotForeground", HideWhenNotForeground);
        HideOutsideNetworkBubble = j.value("HideOutsideNetworkBubble", HideOutsideNetworkBubble);
        DrawWalkableMap = j.value("DrawWalkableMap", DrawWalkableMap);
        // Legacy minimap terrain experiments were removed from runtime, but keep the old
        // key accepted so existing settings files continue to load cleanly.
        (void)j.value("DrawMiniMapTerrain", false);
        DrawMiniMapEntities = j.value("DrawMiniMapEntities", DrawMiniMapEntities);
        WalkableMapBorderThickness =
            std::clamp(j.value("WalkableMapBorderThickness", WalkableMapBorderThickness), 0, 8);
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
    }

    void Save(const std::filesystem::path& pluginDir) const {
        const auto path = pluginDir / "config" / "settings.json";
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        nlohmann::json j;
        j["OverlayEnabled"] = OverlayEnabled;
        j["DrawWhenNotInHideoutOrTown"] = DrawWhenNotInHideoutOrTown;
        j["DrawWhenNotPaused"] = DrawWhenNotPaused;
        j["HideWhenNotForeground"] = HideWhenNotForeground;
        j["HideOutsideNetworkBubble"] = HideOutsideNetworkBubble;
        j["DrawWalkableMap"] = DrawWalkableMap;
        j["DrawMiniMapEntities"] = DrawMiniMapEntities;
        j["WalkableMapBorderThickness"] = WalkableMapBorderThickness;
        j["TerrainStyle"] = static_cast<int>(TerrainStyle);
        j["TerrainHeightMode"] = static_cast<int>(TerrainHeightMode);
        j["DotCellStep"] = DotCellStep;
        j["DotSize"] = DotSize;
        j["WalkableDecimation"] = DotCellStep;
        j["ShowImportantPOI"] = ShowImportantPOI;
        j["DrawPoiIcons"] = DrawPoiIcons;
        j["EnablePOIBackground"] = EnablePOIBackground;
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
