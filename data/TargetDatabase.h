#pragma once

#include "PathMatcher.h"
#include "RadarConfig.h"
#include "RadarLog.h"
#include "RadarTypes.h"

#include <algorithm>
#include <string_view>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

#include "../third_party/json.hpp"

namespace RadarData {

class TargetDatabase {
public:
    std::vector<TargetEntry> storage;
    std::unordered_map<std::string, std::vector<size_t>> byArea;
    std::unordered_map<std::string, std::string>         areaSource;
    std::unordered_map<std::string, std::string>         areaDisplayNames;
    std::vector<size_t> actsGlobalTargets;
    std::vector<size_t> endgameGlobalTargets;
    PatternMatcherSet ignorePatterns;

    static TargetEntry ParseTarget(const nlohmann::json& o, const std::string& category) {
        TargetEntry t;
        t.category = category;
        t.name = o.value("Name", "");
        t.path = o.value("Path", "");
        t.enabled = o.value("Enabled", true);
        t.showIcon = o.value("Icon", false);
        t.iconName = o.value("IconName", "");
        t.iconSize = o.value("IconSize", 30.f);
        t.expectedCount = o.value("ExpectedCount", 1);
        t.markerShape = ParseMarkerShape(o.value("MarkerShape",
                                                 std::string(MarkerShapeName(DefaultTargetMarkerShape()))));
        if (t.markerShape == MarkerShape::None) t.markerShape = DefaultTargetMarkerShape();
        if (o.contains("MarkerColor") && o["MarkerColor"].is_array() && o["MarkerColor"].size() >= 4) {
            auto& a = o["MarkerColor"];
            t.markerColor = {static_cast<uint8_t>(a[0].get<int>()),
                             static_cast<uint8_t>(a[1].get<int>()),
                             static_cast<uint8_t>(a[2].get<int>()),
                             static_cast<uint8_t>(a[3].get<int>())};
        }
        if (o.contains("NameColor") && o["NameColor"].is_string())
            t.nameColor = ParseRgbString(o["NameColor"].get<std::string>(), t.nameColor);
        if (o.contains("BGColor") && o["BGColor"].is_string())
            t.bgColor = ParseRgbString(o["BGColor"].get<std::string>(), t.bgColor);
        if (o.contains("AnchorGridX") && o.contains("AnchorGridY")) {
            t.anchorGridX = o.value("AnchorGridX", 0.f);
            t.anchorGridY = o.value("AnchorGridY", 0.f);
            t.hasAnchor = true;
            t.anchorTileX = o.value("AnchorTileX", 0);
            t.anchorTileY = o.value("AnchorTileY", 0);
        }
        return t;
    }

    void LoadAreaFile(const std::filesystem::path& path, const std::string& category) {
        if (!std::filesystem::exists(path)) return;
        std::ifstream in(path);
        if (!in.is_open()) return;
        nlohmann::json j;
        try {
            in >> j;
            if (!j.is_object()) return;
            for (auto it = j.begin(); it != j.end(); ++it) {
                if (!it.value().is_array()) continue;
                const std::string areaKey = NormalizeAreaKey(it.key());
                std::string display = it.key();
                if (display == "*") display = "Global";
                areaDisplayNames[areaKey] = display;
                if (category != "User" || !areaSource.count(areaKey))
                    areaSource[areaKey] = category;
                for (const auto& entry : it.value()) {
                    size_t idx = storage.size();
                    storage.push_back(ParseTarget(entry, category));
                    if (areaKey == "*" || areaKey == "GLOBAL") {
                        if (category == "Endgame") endgameGlobalTargets.push_back(idx);
                        else actsGlobalTargets.push_back(idx);
                    }
                    else
                        byArea[areaKey].push_back(idx);
                }
            }
        } catch (const std::exception& ex) {
            RadarLog::Instance().Warn("Skipping target file " + path.string() + ": " + ex.what());
        } catch (...) {
            RadarLog::Instance().Warn("Skipping target file " + path.string() + ": parse failed");
        }
    }

    void LoadIgnore(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) return;
        std::ifstream in(path);
        if (!in.is_open()) return;
        nlohmann::json j;
        try {
            in >> j;
            auto loadArr = [&](const nlohmann::json& arr) {
                if (!arr.is_array()) return;
                for (const auto& e : arr) {
                    if (e.is_string()) ignorePatterns.Add(e.get<std::string>());
                    else if (e.is_object() && e.contains("Path"))
                        ignorePatterns.Add(e["Path"].get<std::string>());
                }
            };
            if (j.is_array()) loadArr(j);
            else if (j.is_object())
                for (auto it = j.begin(); it != j.end(); ++it) loadArr(it.value());
        } catch (const std::exception& ex) {
            RadarLog::Instance().Warn("Skipping ignore file " + path.string() + ": " + ex.what());
        } catch (...) {
            RadarLog::Instance().Warn("Skipping ignore file " + path.string() + ": parse failed");
        }
    }

    void Load(const std::filesystem::path& pluginDir) {
        storage.clear();
        byArea.clear();
        areaSource.clear();
        areaDisplayNames.clear();
        actsGlobalTargets.clear();
        endgameGlobalTargets.clear();
        ignorePatterns.patterns.clear();
        LoadAreaFile(pluginDir / "config" / "targets" / "acts.json", "Acts");
        LoadAreaFile(pluginDir / "config" / "targets" / "endgame.json", "Endgame");
        LoadIgnore(pluginDir / "config" / "targets" / "ignore.json");
        const auto userPath = pluginDir / "config" / "targets" / "user.json";
        if (std::filesystem::exists(userPath)) LoadAreaFile(userPath, "User");

        RadarLog::Instance().Info("TargetDatabase loaded: " + std::to_string(storage.size())
                                 + " targets, " + std::to_string(byArea.size()) + " areas");
    }

    bool SaveUser(const std::filesystem::path& pluginDir) const {
        nlohmann::json j = nlohmann::json::object();
        for (const auto& [area, indices] : byArea) {
            nlohmann::json arr = nlohmann::json::array();
            for (size_t idx : indices) {
                if (idx >= storage.size() || storage[idx].category != "User") continue;
                const auto& t = storage[idx];
                arr.push_back({
                    {"Name", t.name},
                    {"Path", t.path},
                    {"Enabled", t.enabled},
                    {"Icon", t.showIcon},
                    {"IconName", t.iconName},
                    {"IconSize", t.iconSize},
                    {"ExpectedCount", t.expectedCount},
                    {"MarkerShape", MarkerShapeName(t.markerShape)},
                    {"MarkerColor", {t.markerColor.r, t.markerColor.g, t.markerColor.b,
                                      t.markerColor.a}},
                    {"NameColor", std::to_string(t.nameColor.r) + ", " + std::to_string(t.nameColor.g)
                                  + ", " + std::to_string(t.nameColor.b)},
                    {"BGColor", std::to_string(t.bgColor.r) + ", " + std::to_string(t.bgColor.g)
                                 + ", " + std::to_string(t.bgColor.b)},
                });
                if (t.hasAnchor) {
                    auto& o = arr.back();
                    o["AnchorGridX"] = t.anchorGridX;
                    o["AnchorGridY"] = t.anchorGridY;
                    if (t.anchorTileX != 0 || t.anchorTileY != 0) {
                        o["AnchorTileX"] = t.anchorTileX;
                        o["AnchorTileY"] = t.anchorTileY;
                    }
                }
            }
            if (!arr.empty()) j[area] = arr;
        }
        const auto path = pluginDir / "config" / "targets" / "user.json";
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            RadarLog::Instance().Warn("Failed to create target config directory: " + ec.message());
            return false;
        }
        std::ofstream out(path);
        if (!out.is_open()) {
            RadarLog::Instance().Warn("Failed to open user target file for save: " + path.string());
            return false;
        }
        out << j.dump(4);
        if (!out.good()) {
            RadarLog::Instance().Warn("Failed to write user target file: " + path.string());
            return false;
        }
        return true;
    }

    static bool SameTargetIdentity(const TargetEntry& a, const TargetEntry& b) {
        if (a.path != b.path) return false;
        if (a.hasAnchor != b.hasAnchor) return !a.hasAnchor && !b.hasAnchor;
        if (!a.hasAnchor) return true;
        return a.anchorTileX == b.anchorTileX && a.anchorTileY == b.anchorTileY
               && a.anchorGridX == b.anchorGridX && a.anchorGridY == b.anchorGridY;
    }

    bool HasUserOverrideFor(const std::string& areaKey, const TargetEntry& candidate) const {
        auto hasMatchIn = [&](const std::vector<size_t>* indices) {
            if (!indices) return false;
            for (size_t idx : *indices) {
                if (idx >= storage.size()) continue;
                const auto& entry = storage[idx];
                if (entry.category != "User") continue;
                if (SameTargetIdentity(entry, candidate)) return true;
            }
            return false;
        };
        if (hasMatchIn(areaKey == "*" || areaKey == "GLOBAL" ? &actsGlobalTargets : nullptr))
            return true;
        if (hasMatchIn(areaKey == "*" || areaKey == "GLOBAL" ? &endgameGlobalTargets : nullptr))
            return true;
        if (const auto it = byArea.find(areaKey); it != byArea.end() && hasMatchIn(&it->second))
            return true;
        return false;
    }

    std::string DisplayNameForArea(std::string_view areaKey) const {
        const std::string key = NormalizeAreaKey(std::string(areaKey));
        if (auto it = areaDisplayNames.find(key); it != areaDisplayNames.end())
            return it->second;
        if (key == "*") return "Global";
        return key;
    }

    bool HasAreaBucket(const std::string& key) const {
        if (key.empty()) return false;
        if (key == "*" || key == "GLOBAL")
            return !actsGlobalTargets.empty() || !endgameGlobalTargets.empty();
        return byArea.find(key) != byArea.end();
    }

    // Host CurrentAreaHash is often a runtime hex id; acts.json keys use zone ids (e.g. P1_4).
    std::string ResolveAreaKey(std::string_view areaHash, std::string_view areaName) const {
        const std::string name = NormalizeAreaKey(std::string(areaName));
        const std::string hash = NormalizeAreaKey(std::string(areaHash));
        if (HasAreaBucket(name)) return name;
        if (HasAreaBucket(hash)) return hash;
        if (!name.empty()) return name;
        return hash;
    }

    std::vector<std::string> ListAreas(const std::string& source) const {
        std::vector<std::string> areas;
        for (const auto& [area, src] : areaSource) {
            if (src != source) continue;
            if (area == "*" || area == "GLOBAL") continue;
            areas.push_back(area);
        }
        std::sort(areas.begin(), areas.end());
        return areas;
    }

    std::vector<const TargetEntry*> GetTargetsForArea(std::string_view areaHash,
                                                    std::string_view areaName = {}) const {
        std::vector<const TargetEntry*> result;
        result.reserve(32);
        auto addIndices = [&](const std::vector<size_t>& indices, const std::string& areaKey) {
            for (size_t i : indices)
                if (i < storage.size() && storage[i].enabled
                    && (storage[i].category == "User" || !HasUserOverrideFor(areaKey, storage[i])))
                    result.push_back(&storage[i]);
        };
        addIndices(actsGlobalTargets, "*");
        const std::string key = ResolveAreaKey(areaHash, areaName);
        const auto srcIt = areaSource.find(key);
        const bool isEndgameArea =
            (srcIt != areaSource.end() && srcIt->second == "Endgame")
            || (key.size() >= 3 && (key.rfind("MAP", 0) == 0 || key.rfind("SANCTUM", 0) == 0));
        if (isEndgameArea) addIndices(endgameGlobalTargets, "*");
        if (auto it = byArea.find(key); it != byArea.end()) addIndices(it->second, key);
        else {
            for (const auto& [k, v] : byArea) {
                if (AreaKeysEqual(k, key)) addIndices(v, k);
            }
        }
        return result;
    }

    void AddUserTarget(const std::string& area, TargetEntry t) {
        t.category = "User";
        const std::string key = NormalizeAreaKey(area);
        size_t idx = storage.size();
        storage.push_back(std::move(t));
        byArea[key].push_back(idx);
        // Keep Acts/Endgame source for bundled areas — only tag new areas as User.
        if (!areaSource.count(key)) {
            areaSource[key] = "User";
            areaDisplayNames[key] = key;
        }
    }

    void UpsertUserTarget(const std::string& area, TargetEntry t, size_t existingStorageIndex = SIZE_MAX) {
        t.category = "User";
        const std::string key = NormalizeAreaKey(area);
        if (existingStorageIndex < storage.size() && storage[existingStorageIndex].category == "User") {
            storage[existingStorageIndex] = std::move(t);
            return;
        }
        if (auto it = byArea.find(key); it != byArea.end()) {
            for (size_t idx : it->second) {
                if (idx >= storage.size() || storage[idx].category != "User") continue;
                if (SameTargetIdentity(storage[idx], t)) {
                    storage[idx] = std::move(t);
                    return;
                }
            }
        }
        AddUserTarget(key, std::move(t));
    }

    std::vector<std::string> ListUserAreas() const { return ListAreas("User"); }

    size_t CountUserTargetsInArea(const std::string& areaKey) const {
        const std::string key = NormalizeAreaKey(areaKey);
        auto it = byArea.find(key);
        if (it == byArea.end()) return 0;
        size_t n = 0;
        for (size_t idx : it->second) {
            if (idx < storage.size() && storage[idx].category == "User") ++n;
        }
        return n;
    }

    bool RemoveUserTargetFromArea(size_t storageIndex, const std::string& areaKey) {
        const std::string key = NormalizeAreaKey(areaKey);
        auto it = byArea.find(key);
        if (it == byArea.end()) return false;
        auto& indices = it->second;
        const auto pos = std::find(indices.begin(), indices.end(), storageIndex);
        if (pos == indices.end()) return false;
        indices.erase(pos);
        if (storageIndex < storage.size()) storage[storageIndex].enabled = false;
        return true;
    }
};

} // namespace RadarData
