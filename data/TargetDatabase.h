#pragma once

#include "EmbeddedTargets.h"
#include "PathMatcher.h"
#include "RadarConfig.h"
#include "RadarLog.h"
#include "RadarTypes.h"

#include <algorithm>
#include <string_view>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "../third_party/json.hpp"

namespace RadarData {

class TargetDatabase {
public:
    struct DefaultFileGenerationResult {
        std::filesystem::path targetDir;
        bool actsGenerated = false;
        bool endgameGenerated = false;
        bool ignoreGenerated = false;
    };

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

    static std::filesystem::path TargetConfigDir(const std::filesystem::path& pluginDir) {
        return pluginDir / "config" / "targets";
    }

    static bool DecodeBase64(std::string_view input, std::string& out) {
        static constexpr unsigned char invalid = 255;
        unsigned char table[256];
        std::fill(table, table + 256, invalid);
        for (unsigned char c = 'A'; c <= 'Z'; ++c) table[c] = c - 'A';
        for (unsigned char c = 'a'; c <= 'z'; ++c) table[c] = c - 'a' + 26;
        for (unsigned char c = '0'; c <= '9'; ++c) table[c] = c - '0' + 52;
        table[static_cast<unsigned char>('+')] = 62;
        table[static_cast<unsigned char>('/')] = 63;

        out.clear();
        out.reserve(input.size() * 3 / 4);
        int val = 0;
        int bits = -8;
        for (unsigned char c : input) {
            if (c == '=') break;
            const unsigned char decoded = table[c];
            if (decoded == invalid) return false;
            val = (val << 6) | decoded;
            bits += 6;
            if (bits >= 0) {
                out.push_back(static_cast<char>((val >> bits) & 0xff));
                bits -= 8;
            }
        }
        return true;
    }

    static bool WriteEmbeddedFileIfMissing(const std::filesystem::path& path,
                                           std::string_view content,
                                           const char* label,
                                           bool* generated = nullptr) {
        std::error_code ec;
        if (generated) *generated = false;
        if (std::filesystem::exists(path, ec)) return true;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            RadarLog::Instance().Warn(std::string("Failed to create directory for ") + label
                                      + ": " + ec.message());
            return false;
        }
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) {
            RadarLog::Instance().Warn(std::string("Failed to create ") + label + ": " + path.string());
            return false;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!out.good()) {
            RadarLog::Instance().Warn(std::string("Failed to write ") + label + ": " + path.string());
            return false;
        }
        if (generated) *generated = true;
        RadarLog::Instance().Info(std::string("Generated default ") + label + ": " + path.string());
        return true;
    }

    static DefaultFileGenerationResult EnsureEmbeddedDefaultsWritten(const std::filesystem::path& pluginDir) {
        DefaultFileGenerationResult result;
        result.targetDir = TargetConfigDir(pluginDir);
        std::string actsJson;
        std::string endgameJson;
        std::string ignoreJson;
        if (DecodeBase64(EmbeddedTargets::kActsJsonBase64, actsJson))
            WriteEmbeddedFileIfMissing(result.targetDir / "acts.json", actsJson,
                                       "acts.json", &result.actsGenerated);
        else
            RadarLog::Instance().Warn("Failed to decode embedded acts.json defaults");
        if (DecodeBase64(EmbeddedTargets::kEndgameJsonBase64, endgameJson))
            WriteEmbeddedFileIfMissing(result.targetDir / "endgame.json", endgameJson,
                                       "endgame.json", &result.endgameGenerated);
        else
            RadarLog::Instance().Warn("Failed to decode embedded endgame.json defaults");
        if (DecodeBase64(EmbeddedTargets::kIgnoreJsonBase64, ignoreJson))
            WriteEmbeddedFileIfMissing(result.targetDir / "ignore.json", ignoreJson,
                                       "ignore.json", &result.ignoreGenerated);
        else
            RadarLog::Instance().Warn("Failed to decode embedded ignore.json defaults");
        return result;
    }

    static bool ReadJsonFileText(const std::filesystem::path& path, std::string& out) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return false;
        std::ostringstream buffer;
        buffer << in.rdbuf();
        out = buffer.str();
        return in.good() || in.eof();
    }

    void LoadAreaJson(const nlohmann::json& j, const std::string& category) {
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
                } else {
                    byArea[areaKey].push_back(idx);
                }
            }
        }
    }

    bool LoadAreaText(std::string_view jsonText, const std::string& category,
                      const std::string& sourceLabel, bool fallbackOnly = false) {
        try {
            const auto parsed = nlohmann::json::parse(jsonText.begin(), jsonText.end());
            LoadAreaJson(parsed, category);
            if (fallbackOnly)
                RadarLog::Instance().Warn("Using embedded default targets in memory for " + sourceLabel);
            return true;
        } catch (const std::exception& ex) {
            RadarLog::Instance().Warn("Failed to parse " + sourceLabel + ": " + ex.what());
        } catch (...) {
            RadarLog::Instance().Warn("Failed to parse " + sourceLabel);
        }
        return false;
    }

    void LoadIgnoreJson(const nlohmann::json& j) {
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
    }

    bool LoadIgnoreText(std::string_view jsonText, const std::string& sourceLabel,
                        bool fallbackOnly = false) {
        try {
            const auto parsed = nlohmann::json::parse(jsonText.begin(), jsonText.end());
            LoadIgnoreJson(parsed);
            if (fallbackOnly)
                RadarLog::Instance().Warn("Using embedded ignore defaults in memory for " + sourceLabel);
            return true;
        } catch (const std::exception& ex) {
            RadarLog::Instance().Warn("Failed to parse " + sourceLabel + ": " + ex.what());
        } catch (...) {
            RadarLog::Instance().Warn("Failed to parse " + sourceLabel);
        }
        return false;
    }

    void LoadAreaFileOrEmbedded(const std::filesystem::path& path, const std::string& category,
                                std::string_view embeddedJson, const char* label) {
        std::string fileText;
        if (ReadJsonFileText(path, fileText) && LoadAreaText(fileText, category, path.string())) return;
        if (std::filesystem::exists(path))
            RadarLog::Instance().Warn(std::string("Falling back to embedded default for corrupt target file: ")
                                      + path.string());
        LoadAreaText(embeddedJson, category, std::string(label), true);
    }

    void LoadIgnoreFileOrEmbedded(const std::filesystem::path& path, std::string_view embeddedJson,
                                  const char* label) {
        std::string fileText;
        if (ReadJsonFileText(path, fileText) && LoadIgnoreText(fileText, path.string())) return;
        if (std::filesystem::exists(path))
            RadarLog::Instance().Warn(std::string("Falling back to embedded default for corrupt ignore file: ")
                                      + path.string());
        LoadIgnoreText(embeddedJson, std::string(label), true);
    }

    void LoadAreaFile(const std::filesystem::path& path, const std::string& category) {
        if (!std::filesystem::exists(path)) return;
        std::ifstream in(path);
        if (!in.is_open()) return;
        nlohmann::json j;
        try {
            in >> j;
            LoadAreaJson(j, category);
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
            LoadIgnoreJson(j);
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
        const auto targetDir = TargetConfigDir(pluginDir);
        std::string actsJson;
        std::string endgameJson;
        std::string ignoreJson;
        if (!DecodeBase64(EmbeddedTargets::kActsJsonBase64, actsJson))
            RadarLog::Instance().Warn("Failed to decode embedded acts.json defaults");
        if (!DecodeBase64(EmbeddedTargets::kEndgameJsonBase64, endgameJson))
            RadarLog::Instance().Warn("Failed to decode embedded endgame.json defaults");
        if (!DecodeBase64(EmbeddedTargets::kIgnoreJsonBase64, ignoreJson))
            RadarLog::Instance().Warn("Failed to decode embedded ignore.json defaults");
        auto countGroupsFor = [&](const std::string& source) {
            size_t count = 0;
            for (const auto& [_, areaSourceName] : areaSource)
                if (areaSourceName == source) ++count;
            return count;
        };
        size_t beforeTargets = storage.size();
        LoadAreaFileOrEmbedded(targetDir / "acts.json", "Acts", actsJson,
                               "embedded acts defaults");
        RadarLog::Instance().Info("Target load Acts: groups=" + std::to_string(countGroupsFor("Acts"))
                                  + " targets=" + std::to_string(storage.size() - beforeTargets));
        beforeTargets = storage.size();
        LoadAreaFileOrEmbedded(targetDir / "endgame.json", "Endgame", endgameJson,
                               "embedded endgame defaults");
        RadarLog::Instance().Info("Target load Endgame: groups=" + std::to_string(countGroupsFor("Endgame"))
                                  + " targets=" + std::to_string(storage.size() - beforeTargets));
        const size_t ignoreBefore = ignorePatterns.patterns.size();
        LoadIgnoreFileOrEmbedded(targetDir / "ignore.json", ignoreJson,
                                 "embedded ignore defaults");
        RadarLog::Instance().Info("Target load Ignore: patterns="
                                  + std::to_string(ignorePatterns.patterns.size() - ignoreBefore));
        const auto userPath = pluginDir / "config" / "targets" / "user.json";
        if (std::filesystem::exists(userPath)) {
            beforeTargets = storage.size();
            LoadAreaFile(userPath, "User");
            RadarLog::Instance().Info("Target load User: groups=" + std::to_string(countGroupsFor("User"))
                                      + " targets=" + std::to_string(storage.size() - beforeTargets));
        }

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
