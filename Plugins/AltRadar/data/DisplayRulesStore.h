#pragma once

#include "IconTables.h"
#include "RadarLog.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace RadarData {

class DisplayRulesStore {
public:
    static constexpr int kSchemaVersion = 4;

    // Changes to the stock rules, applied once to a file older than `version`. After that the file
    // carries the new version, so a rule the user deletes or edits afterwards stays that way.
    struct StockRuleChange {
        int version;
        std::vector<std::string_view> added;  // inserted where a fresh install has them
        std::vector<std::string_view> fixed;  // matching conditions corrected; colour, size, label kept
        std::vector<std::string_view> moved;  // moved to where a fresh install has them
    };
    static inline const std::vector<StockRuleChange> kStockRuleChanges = {
        {2, {"stock.vaal_strongbox", "stock.ezomyte_strongbox", "stock.ventors_strongbox",
             "stock.etched_beetle_strongbox"}, {}, {}},
        // PoE2 0.5.5 data: typed boxes come in Low/High/Expedition tiers, Abyssal boxes live outside
        // StrongBoxes/, the Omen chest path uses forward slashes, and three chest rules sat behind
        // catch-alls that matched first.
        {3, {"stock.legacy_of_the_order", "stock.blacksmiths_strongbox", "stock.cartographers_strongbox",
             "stock.corruption_strongbox", "stock.abyssal_strongbox"},
            {"stock.researchers_strongbox", "stock.jewellers_strongbox", "stock.ornate_strongbox",
             "stock.armourer_strongbox", "stock.arcane_strongbox", "stock.strongbox_large", "stock.omen_chest"},
            {"stock.strongbox_large", "stock.breach_chest", "stock.expedition_chest", "stock.expedition"}},
        // Azmeri wisp deposits were caught by the size-0 rule that hides other objects. Kinds by model.
        {4, {"stock.wisps_vivid", "stock.wisps_primal", "stock.wisps_wild", "stock.wisps_sacred",
             "stock.azmeri_wisps"}, {}, {}},
    };

    static void CopyMatchConditions(const DisplayRule& from, DisplayRule& to) {
        to.categories = from.categories;
        to.subtypes = from.subtypes;
        to.states = from.states;
        to.matchTerms = from.matchTerms;
        to.models = from.models;
        to.mods = from.mods;
        to.rarity = from.rarity;
        to.reaction = from.reaction;
        to.life = from.life;
        to.chest = from.chest;
        to.poi = from.poi;
        to.encounter = from.encounter;
    }

    // Applies every change newer than fileVersion; returns how many rules were added, fixed or moved.
    static size_t UpgradeStockRules(std::vector<DisplayRule>& rules, int fileVersion) {
        const auto defaults = SeededDefaults();
        auto findIn = [](std::vector<DisplayRule>& v, std::string_view id) {
            return std::find_if(v.begin(), v.end(), [&](const DisplayRule& r) { return r.id == id; });
        };
        auto listed = [](const std::vector<std::string_view>& ids, std::string_view id) {
            return std::find(ids.begin(), ids.end(), id) != ids.end();
        };
        size_t changed = 0;
        for (const auto& change : kStockRuleChanges) {
            if (fileVersion >= change.version) continue;
            for (const auto& def : defaults)
                if (listed(change.fixed, def.id))
                    if (auto it = findIn(rules, def.id); it != rules.end()) {
                        CopyMatchConditions(def, *it);
                        ++changed;
                    }
            // Take out the moved rules, then put them and the added ones back in default order,
            // each just before the next default rule the file has.
            std::vector<DisplayRule> pending;
            for (const auto id : change.moved)
                if (auto it = findIn(rules, id); it != rules.end()) {
                    pending.push_back(std::move(*it));
                    rules.erase(it);
                }
            for (size_t i = 0; i < defaults.size(); ++i) {
                const auto& def = defaults[i];
                DisplayRule rule;
                if (auto it = findIn(pending, def.id); it != pending.end()) rule = std::move(*it);
                else if (listed(change.added, def.id) && findIn(rules, def.id) == rules.end()) rule = def;
                else continue;
                auto pos = rules.end();
                for (size_t k = i + 1; k < defaults.size() && pos == rules.end(); ++k)
                    pos = findIn(rules, defaults[k].id);
                rules.insert(pos, std::move(rule));
                ++changed;
            }
        }
        return changed;
    }

    static std::filesystem::path PathFor(const std::filesystem::path& pluginDir) {
        return pluginDir / "config" / "display_rules.json";
    }

    static std::vector<DisplayRule> SeededDefaults() {
        return IconTables::DefaultDisplayRules();
    }

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
        if (!WriteFileAtomic(path, content)) {
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
                                std::string& note, int* fileSchemaVersion = nullptr) {
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
            if (fileSchemaVersion) *fileSchemaVersion = j.value("SchemaVersion", 1);
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
        if (!WriteFileAtomic(path, j.dump(4))) {
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
            int fileSchemaVersion = kSchemaVersion;
            if (TryLoadFromFile(path, loaded, note, &fileSchemaVersion)) {
                if (fileSchemaVersion < kSchemaVersion) {
                    const size_t changed = UpgradeStockRules(loaded, fileSchemaVersion);
                    std::string upgradeNote;
                    Save(pluginDir, loaded, upgradeNote);
                    RadarLog::Instance().Info("display_rules upgraded from schema "
                                              + std::to_string(fileSchemaVersion) + ": "
                                              + std::to_string(changed) + " stock rules changed; " + upgradeNote);
                }
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

            if (!note.empty()) {
                RadarLog::Instance().Warn("display_rules load failed for " + path.string() + ": "
                                          + note + "; using built-in defaults in memory");
                BackupUnreadableFile(path);
            }
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
