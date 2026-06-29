#pragma once

#include "IconTables.h"
#include "RadarConfig.h"

#include <filesystem>
#include <fstream>

#include "../third_party/json.hpp"

namespace RadarData {

inline void TryMigrateFromHost(const std::filesystem::path& pluginDir,
                               const std::filesystem::path& hostExeDir) {
    const auto marker = pluginDir / "config" / ".migrated";
    if (std::filesystem::exists(marker)) return;

    const auto hostCfg = hostExeDir / "Configs" / "radar" / "config.json";
    if (std::filesystem::exists(hostCfg)) {
        std::ifstream in(hostCfg);
        if (in.is_open()) {
            nlohmann::json j;
            in >> j;
            RadarConfig cfg;
            cfg.DrawWhenNotInHideoutOrTown = j.value("DrawWhenNotInHideoutOrTown", true);
            cfg.DrawWhenNotPaused = j.value("DrawWhenNotPaused", true);
            cfg.HideOutsideNetworkBubble = j.value("HideOutsideNetworkBubble", false);
            cfg.DrawWalkableMap = j.value("DrawWalkableMap", true);
            cfg.DrawMiniMapEntities = true;
            cfg.WalkableMapBorderThickness = j.value("WalkableMapBorderThickness", 0);
            cfg.ShowPlayerNames = j.value("ShowPlayersNames", false);
            cfg.ShowImportantPOI = j.value("ShowImportantPOI", true);
            cfg.EnablePOIBackground = j.value("EnablePOIBackground", true);
            cfg.EdgeIndicatorMinimap = j.value("EdgeIndicatorMinimap", true);
            cfg.EdgeIndicatorLargemap = j.value("EdgeIndicatorLargemap", true);
            cfg.LargeMapScaleMultiplier = j.value("LargeMapScaleMultiplier", 0.1738f);
            if (j.contains("WalkableMapColor") && j["WalkableMapColor"].is_array()
                && j["WalkableMapColor"].size() >= 4) {
                auto& a = j["WalkableMapColor"];
                cfg.TextureInteriorColor = ImVec4(a[0].get<float>(), a[1].get<float>(),
                                                  a[2].get<float>(), a[3].get<float>());
                cfg.DotMatrixFillColor = cfg.TextureInteriorColor;
            }
            if (j.contains("POIColor") && j["POIColor"].is_array() && j["POIColor"].size() >= 4) {
                auto& a = j["POIColor"];
                cfg.POIColor = ImVec4(a[0].get<float>(), a[1].get<float>(), a[2].get<float>(),
                                      a[3].get<float>());
            }
            cfg.Save(pluginDir);
        }
    }

    auto copyIf = [&](const char* name) {
        const auto src = hostExeDir / "Resources" / "radar" / name;
        const auto dst = pluginDir / "config" / "targets" / name;
        if (std::filesystem::exists(src) && !std::filesystem::exists(dst))
            std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
    };
    copyIf("acts.json");
    copyIf("endgame.json");
    copyIf("ignore.json");

    const auto hostIcons = hostExeDir / "Resources" / "radar" / "icons.png";
    const auto dstIcons = pluginDir / "assets" / "icons.png";
    if (std::filesystem::exists(hostIcons) && !std::filesystem::exists(dstIcons)) {
        std::error_code ec;
        std::filesystem::create_directories(dstIcons.parent_path(), ec);
        std::filesystem::copy_file(hostIcons, dstIcons);
    }

    std::ofstream(marker).put('1');
}

} // namespace RadarData
