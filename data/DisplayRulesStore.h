#pragma once

#include "IconTables.h"
#include "RadarLog.h"

#include <filesystem>
#include <fstream>
#include <unordered_set>

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

    static bool ValidateRules(const std::vector<DisplayRule>& rules, std::string& note) {
        if (rules.empty()) {
            note = "display_rules.json resolved to zero usable rules";
            return false;
        }
        std::unordered_set<std::string> ids;
        for (const auto& rule : rules) {
            if (rule.id.empty()) {
                note = "display_rules.json contains rule without stable Id";
                return false;
            }
            if (!ids.insert(rule.id).second) {
                note = "display_rules.json contains duplicate Id: " + rule.id;
                return false;
            }
        }
        return true;
    }

    static size_t DuplicateIdCount(const std::vector<DisplayRule>& rules) {
        std::unordered_set<std::string> ids;
        size_t duplicates = 0;
        for (const auto& rule : rules) {
            if (!rule.id.empty() && !ids.insert(rule.id).second) ++duplicates;
        }
        return duplicates;
    }

    static void CountSections(const std::vector<DisplayRule>& rules, size_t& user,
                              size_t& defaults, size_t& stateHides) {
        user = defaults = stateHides = 0;
        for (const auto& rule : rules) {
            if (IconTables::IsStateHideRule(rule)) ++stateHides;
            else if (MarkerShapeNameEquals(rule.source, "Seeded") || IconTables::IsRuneShapeOwnedRule(rule)) ++defaults;
            else ++user;
        }
    }

    static void EnsureStableIds(std::vector<DisplayRule>& rules) {
        std::unordered_set<std::string> used;
        size_t nextUser = 1;
        size_t nextSeeded = 1;
        for (auto& rule : rules) {
            std::string prefix = MarkerShapeNameEquals(rule.source, "Seeded") ? "seeded" : "user";
            size_t& next = prefix == "seeded" ? nextSeeded : nextUser;
            if (rule.id.empty() || used.find(rule.id) != used.end()) {
                do {
                    rule.id = prefix + "." + std::to_string(next++);
                } while (used.find(rule.id) != used.end());
            }
            used.insert(rule.id);
        }
    }

    static bool DecodeDefaultJson(std::string& outJson, std::string& note) {
        if (!DefaultDisplayRules::DecodeJson(outJson)) {
            note = "built-in display rules Base64 decode failed";
            return false;
        }
        return true;
    }

    static bool ValidateDefaultJson(std::string& outJson, std::string& note) {
        try {
            if (!DecodeDefaultJson(outJson, note)) return false;
            const nlohmann::json j = nlohmann::json::parse(outJson);
            if (!j.is_object()) {
                note = "built-in display rules root is not an object";
                return false;
            }
            if (!j.contains("SchemaVersion")) {
                note = "built-in display rules missing SchemaVersion";
                return false;
            }
            if (!j.contains("DisplayRules") || !j["DisplayRules"].is_array()) {
                note = "built-in display rules missing DisplayRules array";
                return false;
            }
            note = "built-in display rules JSON parse ok";
            return true;
        } catch (const std::exception& ex) {
            note = std::string("built-in display rules JSON parse failed: ") + ex.what();
            return false;
        } catch (...) {
            note = "built-in display rules JSON parse failed";
            return false;
        }
    }

    static bool WriteTextFile(const std::filesystem::path& path, std::string_view content,
                              bool overwrite, std::string& note) {
        std::error_code ec;
        if (!overwrite && std::filesystem::exists(path, ec)) {
            note = "display_rules.json already exists: " + path.string();
            return true;
        }
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            note = "create config directory failed for " + path.parent_path().string() + ": " + ec.message();
            return false;
        }
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            note = "open display_rules.json for write failed: " + path.string();
            return false;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!out.good()) {
            note = "write display_rules.json failed: " + path.string();
            return false;
        }
        note = "wrote display_rules.json: " + path.string();
        return true;
    }

    static bool WriteBuiltInDefaults(const std::filesystem::path& pluginDir, bool overwrite,
                                     bool& wroteFile, std::string& note) {
        wroteFile = false;
        const auto path = PathFor(pluginDir);
        std::string defaultJson;
        if (!ValidateDefaultJson(defaultJson, note)) return false;

        auto defaults = SeededDefaults();
        if (!ValidateRules(defaults, note)) {
            note = "built-in display rules validation failed: " + note;
            return false;
        }

        if (!overwrite) {
            std::error_code ec;
            if (std::filesystem::exists(path, ec)) {
                note = "display_rules.json already exists: " + path.string();
                return true;
            }
        }

        if (!WriteTextFile(path, defaultJson, true, note)) return false;
        wroteFile = true;
        return true;
    }

    static std::vector<DisplayRule> SanitizeRules(std::vector<DisplayRule> rules) {
        IconTables::NormalizeDisplayRules(rules);
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
            EnsureStableIds(parsed);
            if (!ValidateRules(parsed, note)) return false;
            outRules = std::move(parsed);
            size_t user = 0, defaults = 0, stateHides = 0;
            CountSections(outRules, user, defaults, stateHides);
            note = "loaded display rules from config/display_rules.json, count="
                   + std::to_string(outRules.size()) + ", user=" + std::to_string(user)
                   + ", defaults=" + std::to_string(defaults)
                   + ", stateHides=" + std::to_string(stateHides);
            return true;
        } catch (const std::exception& ex) {
            note = std::string("display_rules.json parse failed: ") + ex.what();
            return false;
        } catch (...) {
            note = "display_rules.json parse failed";
            return false;
        }
    }

    static bool Save(const std::filesystem::path& pluginDir,
                     const std::vector<DisplayRule>& rules, std::string& note) {
        std::vector<DisplayRule> savedRules = SanitizeRules(rules);
        EnsureStableIds(savedRules);
        nlohmann::json j;
        j["SchemaVersion"] = kSchemaVersion;
        j["DisplayRules"] = nlohmann::json::array();
        for (const auto& rule : savedRules) j["DisplayRules"].push_back(IconTables::WriteDisplayRule(rule));

        const auto path = PathFor(pluginDir);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            note = "create config directory failed for " + path.parent_path().string() + ": " + ec.message();
            return false;
        }
        std::ofstream out(path);
        if (!out.is_open()) {
            note = "open display_rules.json for write failed: " + path.string();
            return false;
        }
        out << j.dump(4);
        if (!out.good()) {
            note = "write display_rules.json failed: " + path.string();
            return false;
        }
        size_t user = 0, defaults = 0, stateHides = 0;
        CountSections(savedRules, user, defaults, stateHides);
        note = "saved display rules to config/display_rules.json, count="
               + std::to_string(savedRules.size()) + ", user=" + std::to_string(user)
               + ", defaults=" + std::to_string(defaults)
               + ", stateHides=" + std::to_string(stateHides);
        return true;
    }

    static void Save(const std::filesystem::path& pluginDir,
                     const std::vector<DisplayRule>& rules) {
        std::string note;
        Save(pluginDir, rules, note);
    }

    static void Load(const std::filesystem::path& pluginDir, std::vector<DisplayRule>& rules) {
        const auto path = PathFor(pluginDir);
        try {
            std::error_code ec;
            const bool exists = std::filesystem::exists(path, ec);
            RadarLog::Instance().Info("Alt Radar displayRulesPath: " + path.string());
            RadarLog::Instance().Info(std::string("display_rules exists: ") + (exists ? "yes" : "no"));

            if (!exists) {
                std::string defaultJson;
                std::string defaultsNote;
                if (!ValidateDefaultJson(defaultJson, defaultsNote)) {
                    RadarLog::Instance().Warn(defaultsNote);
                    rules = SeededDefaults();
                    return;
                }
                RadarLog::Instance().Info(defaultsNote);
                auto defaults = SeededDefaults();
                RadarLog::Instance().Info("default display rule count: " + std::to_string(defaults.size()));
                RadarLog::Instance().Info("duplicate display rule id count: " + std::to_string(DuplicateIdCount(defaults)));
                bool wroteFile = false;
                if (WriteBuiltInDefaults(pluginDir, false, wroteFile, defaultsNote)) {
                    RadarLog::Instance().Info(std::string("generated display_rules: ") + (wroteFile ? "yes" : "no") + "; " + defaultsNote);
                } else {
                    RadarLog::Instance().Warn("generated display_rules: no; " + defaultsNote);
                    rules = std::move(defaults);
                    return;
                }
            } else {
                RadarLog::Instance().Info("generated display_rules: no");
            }

            std::string note;
            std::vector<DisplayRule> loaded;
            if (TryLoadFromFile(path, loaded, note)) {
                RadarLog::Instance().Info("display_rules parse result: ok");
                RadarLog::Instance().Info("loaded display rule count: " + std::to_string(loaded.size()));
                RadarLog::Instance().Info("duplicate display rule id count: " + std::to_string(DuplicateIdCount(loaded)));
                size_t user = 0, defaults = 0, stateHides = 0;
                CountSections(loaded, user, defaults, stateHides);
                if (defaults == 0 || stateHides == 0) {
                    IconTables::ReplaceSeededDefaults(loaded);
                    std::string repairNote;
                    Save(pluginDir, loaded, repairNote);
                    RadarLog::Instance().Warn("display_rules defaults missing; repaired without deleting User Rules");
                    RadarLog::Instance().Info(repairNote);
                }
                rules = std::move(loaded);
                RadarLog::Instance().Info(note);
                return;
            }

            if (!note.empty())
                RadarLog::Instance().Warn("display_rules load failed for " + path.string() + ": "
                                          + note + "; using built-in defaults in memory");
            rules = SeededDefaults();
            RadarLog::Instance().Info("display_rules parse result: failed");
            RadarLog::Instance().Info("default display rule count: " + std::to_string(rules.size()));
        } catch (const std::exception& ex) {
            RadarLog::Instance().Warn("display_rules startup failed for " + path.string() + ": " + ex.what());
            try { rules = SeededDefaults(); } catch (...) { rules.clear(); }
        } catch (...) {
            RadarLog::Instance().Warn("display_rules startup failed for " + path.string() + ": unknown error");
            try { rules = SeededDefaults(); } catch (...) { rules.clear(); }
        }
    }

    static bool RestoreDefaults(const std::filesystem::path& pluginDir,
                                std::vector<DisplayRule>& rules, std::string& note) {
        try {
            IconTables::ReplaceSeededDefaults(rules);
            return Save(pluginDir, rules, note);
        } catch (const std::exception& ex) {
            note = std::string("restore display rules failed for ") + PathFor(pluginDir).string() + ": " + ex.what();
            return false;
        } catch (...) {
            note = std::string("restore display rules failed for ") + PathFor(pluginDir).string() + ": unknown error";
            return false;
        }
    }
};

} // namespace RadarData
