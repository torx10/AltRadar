#pragma once

#include "RadarTypes.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "../third_party/json.hpp"

namespace RadarData {

inline bool IsIconVisible(const IconDef& d) { return d.scale > 0.f; }

struct IconTables {
    static constexpr int kRuleSchemaVersion = 3;

    std::unordered_map<std::string, IconDef> baseIcons;
    std::unordered_map<std::string, IconDef> chestIcons;
    std::unordered_map<std::string, IconDef> breachIcons;
    std::unordered_map<std::string, IconDef> deliriumIcons;
    std::unordered_map<std::string, IconDef> expeditionIcons;
    std::vector<DisplayRule>                 displayRules;
    IconDef                                  poiMonsterDefault{12, 44, 30, MarkerShape::Skull};
    IconDef                                  otherImportantDefault{1, 37, 30, MarkerShape::None};
    int                                      maxCx = 14;
    int                                      maxCy = 71;

    static Rgba8 DefaultMarkerColorForName(std::string_view key,
                                           Rgba8 fallback = Rgba8{}) {
        if (MarkerShapeNameEquals(key, "Self")) return {77, 242, 255, 255};
        if (MarkerShapeNameEquals(key, "Player")) return {77, 242, 255, 255};
        if (MarkerShapeNameEquals(key, "Leader")) return {77, 242, 255, 255};
        if (MarkerShapeNameEquals(key, "Friendly")) return {102, 255, 153, 235};
        if (MarkerShapeNameEquals(key, "NPC")) return {255, 217, 51, 242};
        if (MarkerShapeNameEquals(key, "Special NPC")) return {255, 217, 51, 242};
        if (MarkerShapeNameEquals(key, "Normal Monster")) return {255, 51, 51, 242};
        if (MarkerShapeNameEquals(key, "Magic Monster")) return {115, 166, 255, 247};
        if (MarkerShapeNameEquals(key, "Rare Monster")) return {255, 217, 38, 255};
        if (MarkerShapeNameEquals(key, "Unique Monster")) return {255, 115, 0, 255};
        if (MarkerShapeNameEquals(key, "Pinnacle Boss Not Attackable")) return {255, 115, 0, 255};
        if (MarkerShapeNameEquals(key, "Strongbox")) return {255, 179, 0, 255};
        if (MarkerShapeNameEquals(key, "Jeweller's Strongbox")) return {255, 179, 0, 255};
        if (MarkerShapeNameEquals(key, "Researcher's Strongbox")) return {255, 179, 0, 255};
        if (MarkerShapeNameEquals(key, "Large Strongbox")) return {255, 179, 0, 255};
        if (MarkerShapeNameEquals(key, "Omen Chest")) return {255, 115, 0, 242};
        if (MarkerShapeNameEquals(key, "Rare Chests")) return {255, 217, 38, 242};
        if (MarkerShapeNameEquals(key, "Magic Chests")) return {115, 166, 255, 236};
        if (MarkerShapeNameEquals(key, "All Other Chest")) return {140, 191, 255, 224};
        if (MarkerShapeNameEquals(key, "Breach Chest")) return {242, 89, 242, 255};
        if (MarkerShapeNameEquals(key, "Generic Expedition Chests")) return {38, 230, 217, 255};
        return fallback;
    }

    static MarkerShape DefaultMarkerShapeForName(std::string_view key,
                                                 MarkerShape fallback = MarkerShape::None) {
        if (MarkerShapeNameEquals(key, "Self")) return MarkerShape::Ring;
        if (MarkerShapeNameEquals(key, "Player")) return MarkerShape::Person;
        if (MarkerShapeNameEquals(key, "Leader")) return MarkerShape::Crown;
        if (MarkerShapeNameEquals(key, "Friendly")) return MarkerShape::Person;
        if (MarkerShapeNameEquals(key, "NPC")) return MarkerShape::Chat;
        if (MarkerShapeNameEquals(key, "Special NPC")) return MarkerShape::Chat;
        if (MarkerShapeNameEquals(key, "Normal Monster")) return MarkerShape::Circle;
        if (MarkerShapeNameEquals(key, "Magic Monster")) return MarkerShape::Fang;
        if (MarkerShapeNameEquals(key, "Rare Monster")) return MarkerShape::Claw;
        if (MarkerShapeNameEquals(key, "Unique Monster")) return MarkerShape::Skull;
        if (MarkerShapeNameEquals(key, "Pinnacle Boss Not Attackable")) return MarkerShape::Skull;
        if (MarkerShapeNameEquals(key, "Strongbox")) return MarkerShape::Chest;
        if (MarkerShapeNameEquals(key, "Jeweller's Strongbox")) return MarkerShape::Chest;
        if (MarkerShapeNameEquals(key, "Researcher's Strongbox")) return MarkerShape::Chest;
        if (MarkerShapeNameEquals(key, "Large Strongbox")) return MarkerShape::Chest;
        if (MarkerShapeNameEquals(key, "Omen Chest")) return MarkerShape::Crown;
        if (MarkerShapeNameEquals(key, "Rare Chests")) return MarkerShape::Chest;
        if (MarkerShapeNameEquals(key, "Magic Chests")) return MarkerShape::Chest;
        if (MarkerShapeNameEquals(key, "All Other Chest")) return MarkerShape::Chest;
        if (MarkerShapeNameEquals(key, "Breach Chest")) return MarkerShape::Chest;
        if (MarkerShapeNameEquals(key, "Generic Expedition Chests")) return MarkerShape::Chest;
        return fallback;
    }

    static IconDef ParseIconDef(std::string_view key, const nlohmann::json& o,
                                MarkerShape fallbackShape = MarkerShape::None) {
        const MarkerShape defaultShape = DefaultMarkerShapeForName(key, fallbackShape);
        const MarkerShape parsedShape =
            o.contains("Shape") ? ParseMarkerShape(o.value("Shape", std::string{}))
                                : defaultShape;
        Rgba8 color = DefaultMarkerColorForName(key);
        if (o.contains("Color") && o["Color"].is_array() && o["Color"].size() >= 4) {
            auto& a = o["Color"];
            color = {static_cast<uint8_t>(a[0].get<int>()), static_cast<uint8_t>(a[1].get<int>()),
                     static_cast<uint8_t>(a[2].get<int>()), static_cast<uint8_t>(a[3].get<int>())};
        }
        return IconDef{static_cast<int>(o.value("CX", 0.f)),
                       static_cast<int>(o.value("CY", 0.f)),
                       o.value("Scale", 30.f),
                       parsedShape != MarkerShape::None ? parsedShape : defaultShape,
                       color,
                       o.value("Label", std::string{})};
    }

    static nlohmann::json WriteColor(const Rgba8& color) {
        return {color.r, color.g, color.b, color.a};
    }

    static std::vector<std::string> ReadStringList(const nlohmann::json& j, const char* key) {
        std::vector<std::string> out;
        if (!j.contains(key) || !j[key].is_array()) return out;
        for (const auto& entry : j[key]) {
            if (entry.is_string()) out.push_back(entry.get<std::string>());
        }
        return out;
    }

    static DisplayRule ParseDisplayRule(const nlohmann::json& j) {
        DisplayRule rule;
        rule.enabled = j.value("Enabled", true);
        rule.hide = j.value("Hide", false);
        rule.id = j.value("Id", std::string{});
        rule.source = j.value("Source", std::string{});
        rule.name = j.value("Name", std::string("New rule"));
        rule.categories = ReadStringList(j, "Categories");
        rule.subtypes = ReadStringList(j, "Subtypes");
        rule.states = ReadStringList(j, "States");
        rule.matchTerms = ReadStringList(j, "MatchTerms");
        if (rule.matchTerms.empty()) rule.matchTerms = ReadStringList(j, "Match");
        rule.mods = ReadStringList(j, "Mods");
        rule.rarity = j.value("Rarity", std::string{});
        rule.reaction = j.value("Reaction", std::string{});
        rule.life = j.value("Life", std::string{});
        rule.chest = j.value("Chest", std::string{});
        rule.poi = j.value("Poi", std::string{});
        rule.encounter = j.value("Encounter", std::string{});
        rule.markerShape = ParseMarkerShape(j.value("Shape", std::string("Circle")));
        if (rule.markerShape == MarkerShape::None) rule.markerShape = MarkerShape::Circle;
        rule.useRuneshapeColor = j.value("UseRuneshapeColor", false);
        rule.size = j.value("Size", 6.f);
        rule.label = j.value("Label", std::string{});
        rule.rememberUntilZone = j.value("RememberUntilZone", false);
        rule.navigable = j.value("Navigable", false);
        if (j.contains("Color") && j["Color"].is_array() && j["Color"].size() >= 4) {
            auto& a = j["Color"];
            rule.markerColor = {static_cast<uint8_t>(a[0].get<int>()),
                                static_cast<uint8_t>(a[1].get<int>()),
                                static_cast<uint8_t>(a[2].get<int>()),
                                static_cast<uint8_t>(a[3].get<int>())};
        }
        return rule;
    }

    static nlohmann::json WriteDisplayRule(const DisplayRule& rule) {
        return {{"Enabled", rule.enabled},
                {"Hide", rule.hide},
                {"Id", rule.id},
                {"Source", rule.source},
                {"Name", rule.name},
                {"Categories", rule.categories},
                {"Subtypes", rule.subtypes},
                {"States", rule.states},
                {"MatchTerms", rule.matchTerms},
                {"Mods", rule.mods},
                {"Rarity", rule.rarity},
                {"Reaction", rule.reaction},
                {"Life", rule.life},
                {"Chest", rule.chest},
                {"Poi", rule.poi},
                {"Encounter", rule.encounter},
                {"Shape", MarkerShapeName(rule.markerShape)},
                {"UseRuneshapeColor", rule.useRuneshapeColor},
                {"Color", WriteColor(rule.markerColor)},
                {"Size", rule.size},
                {"Label", rule.label},
                {"RememberUntilZone", rule.rememberUntilZone},
                {"Navigable", rule.navigable}};
    }

    static DisplayRule MakeDisplayRule(const char* id, const char* source, const char* name,
                                       std::initializer_list<const char*> matchTerms,
                                       std::initializer_list<const char*> categories,
                                       std::initializer_list<const char*> subtypes,
                                       std::initializer_list<const char*> states,
                                       MarkerShape shape, Rgba8 color, float size,
                                       const char* label = nullptr, bool hide = false,
                                       bool enabled = true, bool navigable = false) {
        DisplayRule rule;
        rule.enabled = enabled;
        rule.hide = hide;
        rule.id = id;
        rule.source = source;
        rule.name = name;
        for (const char* term : matchTerms) rule.matchTerms.push_back(term);
        for (const char* category : categories) rule.categories.push_back(category);
        for (const char* subtype : subtypes) rule.subtypes.push_back(subtype);
        for (const char* state : states) rule.states.push_back(state);
        rule.markerShape = shape;
        rule.markerColor = color;
        rule.size = size;
        rule.label = label ? label : "";
        rule.navigable = navigable;
        return rule;
    }

    static std::vector<DisplayRule> DefaultDisplayRules() {
        std::vector<DisplayRule> rules;
        const char* seeded = "Seeded";

        DisplayRule hideDead = MakeDisplayRule("stock.hide_dead_monsters", seeded,
                                               "Hide dead monsters", {}, {"Monster"}, {}, {},
                                               MarkerShape::Circle, {255, 217, 38, 255}, 6.f,
                                               nullptr, true);
        hideDead.life = "Dead";
        rules.push_back(std::move(hideDead));

        DisplayRule hideOpened = MakeDisplayRule("stock.hide_opened_chests", seeded,
                                                 "Hide opened chests", {}, {"Chest"}, {}, {},
                                                 MarkerShape::Chest, {255, 217, 38, 255}, 6.f,
                                                 nullptr, true);
        hideOpened.chest = "Opened";
        rules.push_back(std::move(hideOpened));

        rules.push_back(MakeDisplayRule("stock.waypoint", seeded, "Waypoint",
                                        {"MiscellaneousObjects/Waypoint"}, {}, {}, {},
                                        MarkerShape::MapPin, {0, 255, 255, 255}, 7.f, "Waypoint"));
        rules.push_back(MakeDisplayRule("stock.checkpoint", seeded, "Checkpoint",
                                        {"MiscellaneousObjects/Checkpoint"}, {}, {}, {},
                                        MarkerShape::Flag, {0, 204, 255, 255}, 7.f, "Checkpoint"));
        rules.push_back(MakeDisplayRule("stock.entrance", seeded, "Entrance",
                                        {"MiscellaneousObjects/AreaTransition"}, {"Transition"}, {}, {},
                                        MarkerShape::Stairs, {102, 255, 153, 255}, 7.f, "Entrance"));
        rules.push_back(MakeDisplayRule("stock.stash", seeded, "Stash",
                                        {"MiscellaneousObjects/Stash"}, {}, {}, {},
                                        MarkerShape::Chest, {255, 204, 0, 255}, 7.f, "Stash"));
        rules.push_back(MakeDisplayRule("stock.portal", seeded, "Portal",
                                        {"MiscellaneousObjects/TownPortal"}, {}, {}, {},
                                        MarkerShape::Portal, {204, 153, 255, 255}, 7.f, "Portal"));
        rules.push_back(MakeDisplayRule("stock.town_portal", seeded, "Town Portal",
                                        {"MiscellaneousObjects/ReturnToLastTownPortal"}, {}, {}, {},
                                        MarkerShape::Portal, {204, 153, 255, 255}, 7.f, "Town Portal"));
        rules.push_back(MakeDisplayRule("stock.quest_chest", seeded, "Quest Chest",
                                        {"QuestChests"}, {"Chest"}, {}, {},
                                        MarkerShape::Chest, {255, 153, 0, 255}, 7.f, "Quest Chest"));
        rules.push_back(MakeDisplayRule("stock.quest_object", seeded, "Quest Object",
                                        {"QuestObjects"}, {}, {}, {},
                                        MarkerShape::Exclamation, {255, 255, 0, 255}, 7.f, "Quest Object"));
        rules.push_back(MakeDisplayRule("stock.npc_metadata", seeded, "NPC",
                                        {"Monsters/NPC/"}, {"Npc"}, {}, {},
                                        MarkerShape::Chat, {255, 221, 51, 255}, 7.f, "NPC"));
        rules.push_back(MakeDisplayRule("stock.reforging_bench", seeded, "Reforging Bench",
                                        {"ReforgingBench"}, {}, {}, {},
                                        MarkerShape::Coin, {255, 102, 204, 255}, 7.f, "Reforging Bench"));
        rules.push_back(MakeDisplayRule("stock.crafting_bench", seeded, "Crafting Bench",
                                        {"TransmutationBench"}, {}, {}, {},
                                        MarkerShape::Coin, {255, 102, 204, 255}, 7.f, "Crafting Bench"));
        rules.push_back(MakeDisplayRule("stock.quest_marker", seeded, "Quest Marker",
                                        {"EinharQuestMarker"}, {}, {}, {},
                                        MarkerShape::Exclamation, {255, 255, 51, 255}, 7.f, "Quest Marker"));
        rules.push_back(MakeDisplayRule("stock.abyss_crack", seeded, "Abyss Crack",
                                        {"AbyssJumpInteractable"}, {}, {}, {},
                                        MarkerShape::Exclamation, {51, 204, 204, 255}, 7.f, "Abyss Crack"));
        rules.push_back(MakeDisplayRule("stock.expedition", seeded, "Expedition",
                                        {"Expedition2/Expedition2Encounter"}, {"Other"}, {}, {},
                                        MarkerShape::Flag, {38, 230, 217, 255}, 7.f,
                                        "Expedition", false, true, true));
        rules.push_back(MakeDisplayRule("stock.ritual", seeded, "Ritual",
                                        {"Ritual"}, {"Object", "Other"}, {}, {},
                                        MarkerShape::Star, {255, 51, 85, 255}, 7.f, "Ritual"));
        rules.push_back(MakeDisplayRule("stock.breach", seeded, "Breach",
                                        {"Breach"}, {"Object", "Other"}, {}, {},
                                        MarkerShape::Portal, {166, 77, 255, 255}, 7.f, "Breach"));
        rules.push_back(MakeDisplayRule("stock.strongbox_metadata", seeded, "Strongbox",
                                        {"StrongBoxes"}, {"Chest"}, {}, {},
                                        MarkerShape::Chest, {255, 179, 0, 255}, 6.f, "Strongbox"));
        rules.push_back(MakeDisplayRule("stock.essence", seeded, "Essence",
                                        {"Essence"}, {"Object", "Other"}, {}, {},
                                        MarkerShape::Flask, {51, 224, 255, 255}, 7.f, "Essence"));
        rules.push_back(MakeDisplayRule("stock.shrine", seeded, "Shrine",
                                        {"Metadata/Shrines/"}, {}, {}, {},
                                        MarkerShape::Star, {125, 255, 125, 255}, 6.f, "Shrine"));

        rules.push_back(MakeDisplayRule("stock.player_self", seeded, "Self",
                                        {}, {"Player"}, {"PlayerSelf"}, {},
                                        MarkerShape::Ring, {77, 242, 255, 255}, 4.6f));
        rules.push_back(MakeDisplayRule("stock.player_other", seeded, "Player",
                                        {}, {"Player"}, {"PlayerOther"}, {},
                                        MarkerShape::Person, {77, 242, 255, 255}, 3.4f));
        rules.push_back(MakeDisplayRule("stock.player_leader", seeded, "Leader",
                                        {}, {"Player"}, {}, {"PlayerLeader"},
                                        MarkerShape::Crown, {77, 242, 255, 255}, 5.2f));
        rules.push_back(MakeDisplayRule("stock.special_npc", seeded, "Special NPC",
                                        {}, {"Npc"}, {"SpecialNPC"}, {},
                                        MarkerShape::Chat, {255, 217, 51, 242}, 4.8f));
        rules.push_back(MakeDisplayRule("stock.poi_monster", seeded, "POI Monster",
                                        {}, {"Monster"}, {"POIMonster"}, {},
                                        MarkerShape::Skull, {255, 115, 0, 255}, 8.0f));
        rules.push_back(MakeDisplayRule("stock.pinnacle_boss", seeded, "Pinnacle Boss",
                                        {}, {"Monster"}, {"PinnacleBoss"}, {},
                                        MarkerShape::Skull, {255, 115, 0, 255}, 9.0f));
        rules.push_back(MakeDisplayRule("stock.monster_friendly", seeded, "Monster - Friendly",
                                        {}, {"Monster"}, {}, {"MonsterFriendly"},
                                        MarkerShape::Person, {102, 255, 153, 235}, 3.6f));

        DisplayRule monsterNormal = MakeDisplayRule("stock.monster_normal", seeded, "Monster - Normal",
                                                    {}, {"Monster"}, {}, {},
                                                    MarkerShape::Circle, {255, 51, 51, 242}, 2.6f);
        monsterNormal.rarity = "Normal";
        monsterNormal.reaction = "Hostile";
        rules.push_back(std::move(monsterNormal));

        DisplayRule monsterMagic = MakeDisplayRule("stock.monster_magic", seeded, "Monster - Magic",
                                                   {}, {"Monster"}, {}, {},
                                                   MarkerShape::Fang, {115, 166, 255, 247}, 5.5f);
        monsterMagic.rarity = "Magic";
        monsterMagic.reaction = "Hostile";
        rules.push_back(std::move(monsterMagic));

        DisplayRule monsterRare = MakeDisplayRule("stock.monster_rare", seeded, "Monster - Rare",
                                                  {}, {"Monster"}, {}, {},
                                                  MarkerShape::Claw, {255, 217, 38, 255}, 7.5f);
        monsterRare.rarity = "Rare";
        monsterRare.reaction = "Hostile";
        rules.push_back(std::move(monsterRare));

        DisplayRule monsterUnique = MakeDisplayRule("stock.monster_unique", seeded, "Monster - Unique",
                                                    {}, {"Monster"}, {}, {},
                                                    MarkerShape::Skull, {255, 115, 0, 255}, 8.0f);
        monsterUnique.rarity = "Unique";
        monsterUnique.reaction = "Hostile";
        rules.push_back(std::move(monsterUnique));

        DisplayRule chestGeneric = MakeDisplayRule("stock.chest_generic", seeded, "Chest - Generic",
                                                   {}, {"Chest"}, {}, {},
                                                   MarkerShape::Chest, {140, 191, 255, 224}, 4.4f,
                                                   "Chest");
        chestGeneric.chest = "Unopened";
        rules.push_back(std::move(chestGeneric));

        rules.push_back(MakeDisplayRule("stock.strongbox", seeded, "Chest - Strongbox",
                                        {}, {"Chest"}, {"Strongbox"}, {},
                                        MarkerShape::Chest, {255, 179, 0, 255}, 6.0f, "Strongbox"));
        rules.push_back(MakeDisplayRule("stock.strongbox_jeweller", seeded, "Chest - Jeweller",
                                        {}, {"Chest"}, {"JewellerStrongbox"}, {},
                                        MarkerShape::Chest, {255, 179, 0, 255}, 6.0f, "Jeweller Strongbox"));
        rules.push_back(MakeDisplayRule("stock.strongbox_researcher", seeded, "Chest - Researcher",
                                        {}, {"Chest"}, {"ResearcherStrongbox"}, {},
                                        MarkerShape::Chest, {255, 179, 0, 255}, 6.0f, "Researcher Strongbox"));
        rules.push_back(MakeDisplayRule("stock.strongbox_large", seeded, "Chest - Large",
                                        {}, {"Chest"}, {"LargeStrongbox"}, {},
                                        MarkerShape::Chest, {255, 179, 0, 255}, 6.2f, "Large Strongbox"));
        rules.push_back(MakeDisplayRule("stock.omen_chest", seeded, "Chest - Omen",
                                        {}, {"Chest"}, {"OmenChest"}, {},
                                        MarkerShape::Crown, {255, 115, 0, 242}, 5.5f, "Unique Chest"));
        rules.push_back(MakeDisplayRule("stock.chest_rare", seeded, "Chest - Rare",
                                        {}, {"Chest"}, {"ChestRare"}, {},
                                        MarkerShape::Chest, {255, 217, 38, 242}, 5.0f, "Rare Chest"));
        rules.push_back(MakeDisplayRule("stock.chest_magic", seeded, "Chest - Magic",
                                        {}, {"Chest"}, {"ChestMagic"}, {},
                                        MarkerShape::Chest, {115, 166, 255, 236}, 4.4f));
        rules.push_back(MakeDisplayRule("stock.breach_chest", seeded, "Breach Chest",
                                        {}, {"Chest"}, {"BreachChest"}, {},
                                        MarkerShape::Chest, {242, 89, 242, 255}, 5.2f, "Breach Chest"));
        rules.push_back(MakeDisplayRule("stock.expedition_chest", seeded, "Expedition Chest",
                                        {}, {"Chest"}, {"ExpeditionChest"}, {},
                                        MarkerShape::Chest, {38, 230, 217, 255}, 5.2f, "Expedition Chest"));
        rules.push_back(MakeDisplayRule("stock.delirium_bomb", seeded, "Delirium Bomb",
                                        {}, {"Object"}, {}, {},
                                        MarkerShape::None, {255, 255, 255, 255}, 0.f));
        rules.push_back(MakeDisplayRule("stock.delirium_spawner", seeded, "Delirium Spawner",
                                        {}, {"Object"}, {}, {},
                                        MarkerShape::None, {255, 255, 255, 255}, 0.f));
        return rules;
    }

    static void NormalizeDisplayRule(DisplayRule& rule) {
        if (rule.source.empty()) rule.source = "User";
    }

    static bool IsStateHideRule(const DisplayRule& rule) {
        return MarkerShapeNameEquals(rule.source, "Seeded") && rule.hide
               && ((rule.life == "Dead" && !rule.categories.empty() && rule.categories[0] == "Monster")
                   || (rule.chest == "Opened" && !rule.categories.empty() && rule.categories[0] == "Chest"));
    }

    static bool HasDisplayRuleId(const std::vector<DisplayRule>& rules, std::string_view id) {
        for (const auto& rule : rules) {
            if (rule.id == id) return true;
        }
        return false;
    }

    static std::vector<DisplayRule> UserRulesOnly(const std::vector<DisplayRule>& rules) {
        std::vector<DisplayRule> out;
        for (const auto& rule : rules) {
            if (!MarkerShapeNameEquals(rule.source, "Seeded")) out.push_back(rule);
        }
        return out;
    }

    static void ReplaceSeededDefaults(std::vector<DisplayRule>& rules) {
        std::vector<DisplayRule> merged;
        for (const auto& def : DefaultDisplayRules()) {
            if (IsStateHideRule(def)) merged.push_back(def);
        }
        for (const auto& rule : rules) {
            if (!MarkerShapeNameEquals(rule.source, "Seeded")) merged.push_back(rule);
        }
        for (const auto& def : DefaultDisplayRules()) {
            if (!IsStateHideRule(def)) merged.push_back(def);
        }
        rules = std::move(merged);
    }

    static void LoadMap(const nlohmann::json& j,
                        std::unordered_map<std::string, IconDef>& out,
                        int& maxCx, int& maxCy) {
        if (!j.is_object()) return;
        for (auto it = j.begin(); it != j.end(); ++it) {
            IconDef d = ParseIconDef(it.key(), it.value());
            out[it.key()] = d;
            maxCx = std::max(maxCx, d.cx + 1);
            maxCy = std::max(maxCy, d.cy + 1);
        }
    }

    void Load(const std::filesystem::path& pluginDir) {
        baseIcons.clear();
        chestIcons.clear();
        breachIcons.clear();
        deliriumIcons.clear();
        expeditionIcons.clear();
        poiMonsterDefault = IconDef{12, 44, 30, MarkerShape::Skull};
        otherImportantDefault = IconDef{1, 37, 30, MarkerShape::None};
        maxCx = 14;
        maxCy = 71;
        const auto path = pluginDir / "config" / "icons.json";
        if (!std::filesystem::exists(path)) {
            SeedIconDefaults();
            return;
        }
        std::ifstream in(path);
        if (!in.is_open()) {
            SeedIconDefaults();
            return;
        }
        nlohmann::json j;
        try {
            in >> j;
        } catch (...) {
            SeedIconDefaults();
            return;
        }
        LoadMap(j.value("BaseIcons", nlohmann::json::object()), baseIcons, maxCx, maxCy);
        LoadMap(j.value("ChestIcons", nlohmann::json::object()), chestIcons, maxCx, maxCy);
        LoadMap(j.value("BreachIcons", nlohmann::json::object()), breachIcons, maxCx, maxCy);
        LoadMap(j.value("DeliriumIcons", nlohmann::json::object()), deliriumIcons, maxCx, maxCy);
        LoadMap(j.value("ExpeditionIcons", nlohmann::json::object()), expeditionIcons, maxCx, maxCy);
        if (j.contains("POIMonsters"))
            for (auto& [k, v] : j["POIMonsters"].items()) {
                poiMonsterDefault = ParseIconDef(k, v, MarkerShape::Skull);
                break;
            }
        if (j.contains("OtherImportantObjects"))
            for (auto& [k, v] : j["OtherImportantObjects"].items()) {
                otherImportantDefault = ParseIconDef(k, v, MarkerShape::None);
                break;
            }
    }

    void Save(const std::filesystem::path& pluginDir) const {
        auto writeMap = [](const std::unordered_map<std::string, IconDef>& m) {
            nlohmann::json o = nlohmann::json::object();
            for (const auto& [k, d] : m)
                o[k] = {{"CX", d.cx},
                        {"CY", d.cy},
                        {"Scale", d.scale},
                        {"Shape", MarkerShapeName(d.markerShape)},
                        {"Color", WriteColor(d.markerColor)},
                        {"Label", d.label}};
            return o;
        };
        nlohmann::json j;
        j["BaseIcons"] = writeMap(baseIcons);
        j["ChestIcons"] = writeMap(chestIcons);
        j["BreachIcons"] = writeMap(breachIcons);
        j["DeliriumIcons"] = writeMap(deliriumIcons);
        j["ExpeditionIcons"] = writeMap(expeditionIcons);
        j["POIMonsters"] = {{"-1", {{"CX", poiMonsterDefault.cx},
                                       {"CY", poiMonsterDefault.cy},
                                      {"Scale", poiMonsterDefault.scale},
                                      {"Shape", MarkerShapeName(poiMonsterDefault.markerShape)},
                                      {"Color", WriteColor(poiMonsterDefault.markerColor)},
                                      {"Label", poiMonsterDefault.label}}}};
        j["OtherImportantObjects"] = {
            {"-1", {{"CX", otherImportantDefault.cx},
                    {"CY", otherImportantDefault.cy},
                    {"Scale", otherImportantDefault.scale},
                    {"Shape", MarkerShapeName(otherImportantDefault.markerShape)},
                    {"Color", WriteColor(otherImportantDefault.markerColor)},
                    {"Label", otherImportantDefault.label}}}};
        const auto path = pluginDir / "config" / "icons.json";
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream out(path);
        if (out.is_open()) out << j.dump(4);
    }

    void SeedIconDefaults() {
        auto put = [](auto& m, const char* n, int cx, int cy, float sc,
                      MarkerShape shape = MarkerShape::None,
                      Rgba8 color = {}) {
            m[n] = IconDef{cx, cy, sc, shape, color, {}};
        };
        put(baseIcons, "Self", 0, 0, 15, MarkerShape::Ring, DefaultMarkerColorForName("Self"));
        put(baseIcons, "Player", 2, 0, 15, MarkerShape::Person, DefaultMarkerColorForName("Player"));
        put(baseIcons, "Leader", 3, 1, 25, MarkerShape::Crown, DefaultMarkerColorForName("Leader"));
        put(baseIcons, "Friendly", 1, 0, 15, MarkerShape::Person, DefaultMarkerColorForName("Friendly"));
        put(baseIcons, "NPC", 3, 0, 15, MarkerShape::Chat, DefaultMarkerColorForName("NPC"));
        put(baseIcons, "Special NPC", 13, 42, 30, MarkerShape::Chat, DefaultMarkerColorForName("Special NPC"));
        put(baseIcons, "Normal Monster", 0, 14, 30, MarkerShape::Circle, DefaultMarkerColorForName("Normal Monster"));
        put(baseIcons, "Magic Monster", 6, 3, 30, MarkerShape::Fang, DefaultMarkerColorForName("Magic Monster"));
        put(baseIcons, "Rare Monster", 4, 57, 30, MarkerShape::Claw, DefaultMarkerColorForName("Rare Monster"));
        put(baseIcons, "Unique Monster", 6, 57, 30, MarkerShape::Skull, DefaultMarkerColorForName("Unique Monster"));
        put(baseIcons, "Pinnacle Boss Not Attackable", 6, 6, 30, MarkerShape::Skull, DefaultMarkerColorForName("Pinnacle Boss Not Attackable"));
        put(baseIcons, "Shrine", 7, 0, 30, MarkerShape::MapPin, {64, 235, 255, 255});
        put(baseIcons, "Waypoint", 7, 0, 30, MarkerShape::MapPin, {64, 235, 255, 255});
        put(baseIcons, "Checkpoint", 13, 42, 30, MarkerShape::Flag, {255, 217, 51, 255});
        put(baseIcons, "Transition", 10, 2, 30, MarkerShape::Stairs, {102, 255, 153, 255});
        put(chestIcons, "Strongbox", 1, 58, 30, MarkerShape::Chest, DefaultMarkerColorForName("Strongbox"));
        put(chestIcons, "Jeweller's Strongbox", 11, 60, 35, MarkerShape::Chest, DefaultMarkerColorForName("Jeweller's Strongbox"));
        put(chestIcons, "Researcher's Strongbox", 6, 62, 35, MarkerShape::Chest, DefaultMarkerColorForName("Researcher's Strongbox"));
        put(chestIcons, "Large Strongbox", 8, 62, 35, MarkerShape::Chest, DefaultMarkerColorForName("Large Strongbox"));
        put(chestIcons, "Omen Chest", 3, 58, 40, MarkerShape::Crown, DefaultMarkerColorForName("Omen Chest"));
        put(chestIcons, "Rare Chests", 1, 70, 30, MarkerShape::Chest, DefaultMarkerColorForName("Rare Chests"));
        put(chestIcons, "Magic Chests", 2, 70, 25, MarkerShape::Chest, DefaultMarkerColorForName("Magic Chests"));
        put(chestIcons, "All Other Chest", 10, 10, 15, MarkerShape::Chest, DefaultMarkerColorForName("All Other Chest"));
        put(breachIcons, "Breach Chest", 1, 2, 20, MarkerShape::Chest, DefaultMarkerColorForName("Breach Chest"));
        put(deliriumIcons, "Delirium Bomb", 11, 37, 15);
        put(deliriumIcons, "Delirium Spawner", 1, 40, 15);
        put(expeditionIcons, "Cavern Entrance", 0, 2, 50);
        put(expeditionIcons, "Chest Quantity Remnant", 11, 40, 60);
        put(expeditionIcons, "Logbook", 4, 40, 50);
        put(expeditionIcons, "Splinter Chest", 4, 40, 55);
        maxCx = 14;
        maxCy = 71;
    }
};

} // namespace RadarData
