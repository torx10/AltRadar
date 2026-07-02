#pragma once

#include "IconTables.h"
#include "RadarLog.h"

#include <filesystem>
#include <fstream>

namespace RadarData {

class DisplayRulesStore {
public:
    static constexpr int kSchemaVersion = 1;

    static std::filesystem::path PathFor(const std::filesystem::path& pluginDir) {
        return pluginDir / "config" / "display_rules.json";
    }

    static std::vector<DisplayRule> SeededDefaults() {
        return IconTables::DefaultDisplayRules();
    }

    static bool HasUsableRules(const std::vector<DisplayRule>& rules) { return !rules.empty(); }

    static std::vector<DisplayRule> SanitizeRules(std::vector<DisplayRule> rules) {
        for (auto& rule : rules) IconTables::NormalizeDisplayRule(rule);
        return rules;
    }

    static bool TryLoadFromFile(const std::filesystem::path& path, std::vector<DisplayRule>& outRules,
                                std::string& note) {
        if (!std::filesystem::exists(path)) return false;
        std::ifstream in(path);
        if (!in.is_open()) {
            note = "display_rules.json unreadable";
            return false;
        }

        try {
            nlohmann::json j;
            in >> j;
            if (!j.is_object()) {
                note = "display_rules.json root is not an object";
                return false;
            }
            if (!j.contains("DisplayRules") || !j["DisplayRules"].is_array()) {
                note = "display_rules.json missing DisplayRules array";
                return false;
            }

            std::vector<DisplayRule> parsed;
            for (const auto& entry : j["DisplayRules"])
                parsed.push_back(IconTables::ParseDisplayRule(entry));
            parsed = SanitizeRules(std::move(parsed));
            if (!HasUsableRules(parsed)) {
                note = "display_rules.json resolved to zero usable rules";
                return false;
            }
            outRules = std::move(parsed);
            note = "Loaded display rules from config/display_rules.json";
            return true;
        } catch (const std::exception& ex) {
            note = std::string("display_rules.json parse failed: ") + ex.what();
            return false;
        } catch (...) {
            note = "display_rules.json parse failed";
            return false;
        }
    }

    static bool TryLoadLegacyIconsRules(const std::filesystem::path& pluginDir,
                                        std::vector<DisplayRule>& outRules,
                                        std::string& note) {
        const auto path = pluginDir / "config" / "icons.json";
        if (!std::filesystem::exists(path)) return false;
        std::ifstream in(path);
        if (!in.is_open()) return false;

        try {
            nlohmann::json j;
            in >> j;
            if (!j.is_object() || !j.contains("DisplayRules") || !j["DisplayRules"].is_array())
                return false;

            std::vector<DisplayRule> parsed;
            for (const auto& entry : j["DisplayRules"])
                parsed.push_back(IconTables::ParseDisplayRule(entry));
            parsed = SanitizeRules(std::move(parsed));
            if (!HasUsableRules(parsed)) return false;
            outRules = std::move(parsed);
            note = "Migrated legacy rules from config/icons.json";
            return true;
        } catch (...) {
            return false;
        }
    }

    static void Save(const std::filesystem::path& pluginDir,
                     const std::vector<DisplayRule>& rules) {
        nlohmann::json j;
        j["SchemaVersion"] = kSchemaVersion;
        j["DisplayRules"] = nlohmann::json::array();
        for (const auto& rule : rules) j["DisplayRules"].push_back(IconTables::WriteDisplayRule(rule));

        const auto path = PathFor(pluginDir);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream out(path);
        if (out.is_open()) out << j.dump(4);
    }

    static void Load(const std::filesystem::path& pluginDir, std::vector<DisplayRule>& rules) {
        std::string note;
        std::vector<DisplayRule> loaded;
        if (TryLoadFromFile(PathFor(pluginDir), loaded, note)) {
            rules = std::move(loaded);
            RadarLog::Instance().Info(note);
            return;
        }

        if (!note.empty()) RadarLog::Instance().Warn(note + "; seeding or migrating rules");

        if (TryLoadLegacyIconsRules(pluginDir, loaded, note)) {
            rules = std::move(loaded);
            Save(pluginDir, rules);
            RadarLog::Instance().Info(note);
            return;
        }

        rules = SeededDefaults();
        Save(pluginDir, rules);
        RadarLog::Instance().Info("Seeded default display rules into config/display_rules.json");
    }
};

} // namespace RadarData
