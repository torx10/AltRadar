#pragma once

#include "RadarConfig.h"
#include "RadarLog.h"
#include "RadarTypes.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>

#include "../third_party/json.hpp"

namespace RadarData {

struct TargetOverrideEntry {
    std::string key;
    std::optional<bool> enabled;
    std::optional<std::string> name;
    std::optional<std::string> path;
    std::optional<bool> showIcon;
    std::optional<std::string> iconName;
    std::optional<float> iconSize;
    std::optional<int> expectedCount;
    std::optional<MarkerShape> markerShape;
    std::optional<Rgba8> markerColor;
    std::optional<Rgba8> nameColor;
    std::optional<Rgba8> bgColor;
};

class TargetOverridesStore {
public:
    std::unordered_map<std::string, TargetOverrideEntry> entries;

    static std::filesystem::path Path(const std::filesystem::path& pluginDir) {
        return pluginDir / "config" / "targets" / "overrides.json";
    }

    void Clear() { entries.clear(); }

    bool Load(const std::filesystem::path& pluginDir) {
        entries.clear();
        const auto path = Path(pluginDir);
        if (!std::filesystem::exists(path)) return true;
        std::ifstream in(path);
        if (!in.is_open()) {
            RadarLog::Instance().Warn("Failed to open target overrides file: " + path.string());
            return false;
        }
        try {
            nlohmann::json j;
            in >> j;
            if (!j.is_object() || j.value("SchemaVersion", 0) != 1 || !j.contains("Overrides")
                || !j["Overrides"].is_array()) {
                RadarLog::Instance().Warn("Ignoring malformed target overrides file: " + path.string());
                return false;
            }
            for (const auto& o : j["Overrides"]) {
                if (!o.is_object() || !o.contains("Key") || !o["Key"].is_string()) continue;
                TargetOverrideEntry e;
                e.key = o["Key"].get<std::string>();
                if (e.key.empty()) continue;
                if (o.contains("Enabled") && o["Enabled"].is_boolean()) e.enabled = o["Enabled"].get<bool>();
                if (o.contains("Name") && o["Name"].is_string()) e.name = o["Name"].get<std::string>();
                if (o.contains("Path") && o["Path"].is_string()) e.path = o["Path"].get<std::string>();
                if (o.contains("Icon") && o["Icon"].is_boolean()) e.showIcon = o["Icon"].get<bool>();
                if (o.contains("IconName") && o["IconName"].is_string()) e.iconName = o["IconName"].get<std::string>();
                if (o.contains("IconSize") && o["IconSize"].is_number()) e.iconSize = o["IconSize"].get<float>();
                if (o.contains("ExpectedCount") && o["ExpectedCount"].is_number_integer())
                    e.expectedCount = o["ExpectedCount"].get<int>();
                if (o.contains("MarkerShape") && o["MarkerShape"].is_string()) {
                    const auto shape = ParseMarkerShape(o["MarkerShape"].get<std::string>());
                    if (shape != MarkerShape::None) e.markerShape = shape;
                }
                if (o.contains("MarkerColor") && o["MarkerColor"].is_array() && o["MarkerColor"].size() >= 4) {
                    const auto& a = o["MarkerColor"];
                    e.markerColor = Rgba8{static_cast<uint8_t>(a[0].get<int>()),
                                          static_cast<uint8_t>(a[1].get<int>()),
                                          static_cast<uint8_t>(a[2].get<int>()),
                                          static_cast<uint8_t>(a[3].get<int>())};
                }
                if (o.contains("NameColor") && o["NameColor"].is_string())
                    e.nameColor = ParseRgbString(o["NameColor"].get<std::string>());
                if (o.contains("BGColor") && o["BGColor"].is_string())
                    e.bgColor = ParseRgbString(o["BGColor"].get<std::string>());
                entries[e.key] = std::move(e);
            }
            return true;
        } catch (const std::exception& ex) {
            RadarLog::Instance().Warn("Ignoring malformed target overrides file " + path.string() + ": " + ex.what());
        } catch (...) {
            RadarLog::Instance().Warn("Ignoring malformed target overrides file: " + path.string());
        }
        entries.clear();
        return false;
    }

    bool Save(const std::filesystem::path& pluginDir) const {
        const auto path = Path(pluginDir);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            RadarLog::Instance().Warn("Failed to create target config directory: " + ec.message());
            return false;
        }
        nlohmann::json j;
        j["SchemaVersion"] = 1;
        j["Overrides"] = nlohmann::json::array();
        for (const auto& [_, e] : entries) {
            nlohmann::json o;
            o["Key"] = e.key;
            if (e.enabled) o["Enabled"] = *e.enabled;
            if (e.name) o["Name"] = *e.name;
            if (e.path) o["Path"] = *e.path;
            if (e.showIcon) o["Icon"] = *e.showIcon;
            if (e.iconName) o["IconName"] = *e.iconName;
            if (e.iconSize) o["IconSize"] = *e.iconSize;
            if (e.expectedCount) o["ExpectedCount"] = *e.expectedCount;
            if (e.markerShape) o["MarkerShape"] = MarkerShapeName(*e.markerShape);
            if (e.markerColor) o["MarkerColor"] = {e.markerColor->r, e.markerColor->g, e.markerColor->b, e.markerColor->a};
            if (e.nameColor) o["NameColor"] = ColorString(*e.nameColor);
            if (e.bgColor) o["BGColor"] = ColorString(*e.bgColor);
            j["Overrides"].push_back(std::move(o));
        }
        std::ofstream out(path);
        if (!out.is_open()) {
            RadarLog::Instance().Warn("Failed to open target overrides file for save: " + path.string());
            return false;
        }
        out << j.dump(4);
        return out.good();
    }

    static bool Reset(const std::filesystem::path& pluginDir) {
        std::error_code ec;
        std::filesystem::remove(Path(pluginDir), ec);
        if (ec) {
            RadarLog::Instance().Warn("Failed to remove target overrides file: " + ec.message());
            return false;
        }
        return true;
    }

private:
    static std::string ColorString(const Rgba8& c) {
        return std::to_string(c.r) + ", " + std::to_string(c.g) + ", " + std::to_string(c.b);
    }
};

} // namespace RadarData
