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
    bool  DrawMiniMapTerrain = true;
    bool  DrawMiniMapEntities = true;
    int   WalkableMapBorderThickness = 0;
    TerrainTextureAlignmentMode TerrainAlignment = TerrainTextureAlignmentMode::Legacy;
    int   WalkableDecimation = 4;
    bool  ShowPlayerNames = false;
    bool  ShowImportantPOI = true;
    bool  DrawPoiIcons = false;
    bool  EnablePOIBackground = true;
    bool  EdgeIndicatorMinimap = true;
    bool  EdgeIndicatorLargemap = true;
    bool  UseLegacyClassifier = false;
    float LargeMapScaleMultiplier = 0.1738f;
    ImVec4 WalkableMapInteriorColor{0.46f, 0.46f, 0.46f, 0.7f};
    ImVec4 WalkableMapEdgeColor{60.0f / 255.0f, 220.0f / 255.0f, 1.0f, 180.0f / 255.0f};
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
        DrawMiniMapTerrain = j.value("DrawMiniMapTerrain", DrawMiniMapTerrain);
        DrawMiniMapEntities = j.value("DrawMiniMapEntities", DrawMiniMapEntities);
        WalkableMapBorderThickness =
            std::clamp(j.value("WalkableMapBorderThickness", WalkableMapBorderThickness), 0, 8);
        TerrainAlignment = static_cast<TerrainTextureAlignmentMode>(
            std::clamp(j.value("TerrainAlignment", static_cast<int>(TerrainAlignment)), 0, 2));
        WalkableDecimation = std::clamp(j.value("WalkableDecimation", WalkableDecimation), 2, 16);
        ShowPlayerNames = j.value("ShowPlayerNames", ShowPlayerNames);
        ShowImportantPOI = j.value("ShowImportantPOI", ShowImportantPOI);
        DrawPoiIcons = j.value("DrawPoiIcons", DrawPoiIcons);
        EnablePOIBackground = j.value("EnablePOIBackground", EnablePOIBackground);
        EdgeIndicatorMinimap = j.value("EdgeIndicatorMinimap", EdgeIndicatorMinimap);
        EdgeIndicatorLargemap = j.value("EdgeIndicatorLargemap", EdgeIndicatorLargemap);
        UseLegacyClassifier = j.value("UseLegacyClassifier", UseLegacyClassifier);
        LargeMapScaleMultiplier = j.value("LargeMapScaleMultiplier", LargeMapScaleMultiplier);
        MaxEntitiesDrawn = std::clamp(j.value("MaxEntitiesDrawn", MaxEntitiesDrawn), 64, 4096);
        const bool hasInteriorColor = readColor("WalkableMapInteriorColor", WalkableMapInteriorColor);
        if (!hasInteriorColor)
            readColor("WalkableMapColor", WalkableMapInteriorColor);
        readColor("WalkableMapEdgeColor", WalkableMapEdgeColor);
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
        j["DrawMiniMapTerrain"] = DrawMiniMapTerrain;
        j["DrawMiniMapEntities"] = DrawMiniMapEntities;
        j["WalkableMapBorderThickness"] = WalkableMapBorderThickness;
        j["TerrainAlignment"] = static_cast<int>(TerrainAlignment);
        j["WalkableDecimation"] = WalkableDecimation;
        j["ShowPlayerNames"] = ShowPlayerNames;
        j["ShowImportantPOI"] = ShowImportantPOI;
        j["DrawPoiIcons"] = DrawPoiIcons;
        j["EnablePOIBackground"] = EnablePOIBackground;
        j["EdgeIndicatorMinimap"] = EdgeIndicatorMinimap;
        j["EdgeIndicatorLargemap"] = EdgeIndicatorLargemap;
        j["UseLegacyClassifier"] = UseLegacyClassifier;
        j["LargeMapScaleMultiplier"] = LargeMapScaleMultiplier;
        j["MaxEntitiesDrawn"] = MaxEntitiesDrawn;
        j["WalkableMapInteriorColor"] = {WalkableMapInteriorColor.x, WalkableMapInteriorColor.y,
                                          WalkableMapInteriorColor.z, WalkableMapInteriorColor.w};
        j["WalkableMapEdgeColor"] = {WalkableMapEdgeColor.x, WalkableMapEdgeColor.y,
                                      WalkableMapEdgeColor.z, WalkableMapEdgeColor.w};
        j["WalkableMapColor"] = {WalkableMapInteriorColor.x, WalkableMapInteriorColor.y,
                                  WalkableMapInteriorColor.z, WalkableMapInteriorColor.w};
        j["POIColor"] = {POIColor.x, POIColor.y, POIColor.z, POIColor.w};
        j["MainMenuSize"] = {MainMenuSize.x, MainMenuSize.y};
        std::ofstream out(path);
        if (out.is_open()) out << j.dump(4);
    }
};

} // namespace RadarData
