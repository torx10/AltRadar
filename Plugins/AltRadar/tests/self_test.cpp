// Self-checks for AltRadar's file and pattern logic. No game or host needed.
// Build and run from the repo root (CI does this before every build):
//   g++ -std=c++20 -IPlugins/AltRadar -IPOEFixer/imgui Plugins/AltRadar/tests/self_test.cpp -o self_test && ./self_test
#include <ctime>
#ifndef _WIN32
// Map the MSVC-only functions the data code uses, so this also runs on Linux.
#include <cstdio>
#include <cstring>
#include <thread>
#define localtime_s(tmOut, timeIn) localtime_r(timeIn, tmOut)
#define sscanf_s sscanf  // only used with %d, where the two behave the same
#endif

#undef NDEBUG
#include "data/DisplayRulesStore.h"
#include "data/IconTables.h"
#include "data/PathField.h"
#include "data/PathService.h"
#include "data/PathMatcher.h"
#include "data/RadarConfig.h"
#include "data/RoomTargets.h"
#include "data/TargetDatabase.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using namespace RadarData;

static bool Matches(const char* pattern, const char* candidate) {
    return MatchPattern(CompilePattern(pattern), candidate);
}

static std::string ReadAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void WriteRaw(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

static void TestPathMatcher() {
    // Patterns from the shipped acts/endgame/ignore files that the old matcher missed.
    assert(Matches("*RitualRuneObject*", "Metadata/Terrain/Leagues/Ritual/RitualRuneObject"));
    assert(Matches("*RitualRuneObject*", "Metadata/MiscellaneousObjects/Ritual/RitualRuneObject_Interactable"));
    assert(Matches("*Doodad*", "Metadata/Terrain/Doodads/RockDoodad"));
    assert(Matches("Metadata/*/Boss", "Metadata/Monsters/Act1/Boss"));
    assert(Matches("Metadata/Terrain/Desert/BuriedCity/Features/QueenKabalaRoom_*_meta*",
                   "Metadata/Terrain/Desert/BuriedCity/Features/QueenKabalaRoom_a_b_meta.tdtx"));
    // Still no false matches.
    assert(!Matches("*RitualRuneObject*", "Metadata/Terrain/Leagues/Ritual/RitualAltar"));
    assert(!Matches("Metadata/*/Boss", "Metadata/Monsters/BossMinion"));
    assert(!Matches("*Doodad*", "Metadata/Chests/Strongbox"));
    // Behaviour kept from before: case-insensitive, extension and tile-suffix rules,
    // and a lone trailing '*' matching as prefix or suffix.
    assert(Matches("metadata/chests/strongbox", "Metadata/Chests/Strongbox"));
    assert(Matches("Metadata/Terrain/Arena.tdtx:[]", "Metadata/Terrain/Arena.tdbx:1-y:1"));
    assert(!Matches("Metadata/Terrain/Arena:1-y:1", "Metadata/Terrain/Arena:2-y:1"));
    assert(Matches("Metadata/Monsters/Boss*", "Metadata/Monsters/BossPhase2"));
    assert(Matches("AldurRuneEncounterController*", "Metadata/Leagues/AldurRuneEncounterController"));
    assert(!Matches("", "anything"));
}

static void TestWriteFileAtomic(const fs::path& dir) {
    const auto p = dir / "atomic.json";
    assert(WriteFileAtomic(p, "first"));
    assert(WriteFileAtomic(p, "second"));  // replaces an existing file
    assert(ReadAll(p) == "second");
    assert(!fs::exists(dir / "atomic.json.tmp"));
    assert(!WriteFileAtomic(dir / "missing-dir" / "x.json", "x"));  // fails cleanly
}

// A broken user.json used to be replaced by an empty list on the next save.
static void TestBrokenUserTargetsAreKept(const fs::path& pluginDir) {
    const auto user = pluginDir / "config" / "targets" / "user.json";
    const std::string broken = "{ \"G1_1\": [ { \"Name\": \"My spot\", \"Path\": \"x\" } ";  // truncated
    WriteRaw(user, broken);

    TargetDatabase db;
    db.Load(pluginDir);
    db.SaveUser(pluginDir);

    assert(fs::exists(pluginDir / "config" / "targets" / "user.json.bad"));
    assert(ReadAll(pluginDir / "config" / "targets" / "user.json.bad") == broken);
}

static void TestBrokenSettingsAreKept(const fs::path& pluginDir) {
    const auto settings = pluginDir / "config" / "settings.json";
    const std::string broken = "{ \"OverlayEnabled\": tru";
    WriteRaw(settings, broken);

    RadarConfig cfg;
    cfg.Load(pluginDir);
    cfg.Save(pluginDir);

    assert(ReadAll(pluginDir / "config" / "settings.json.bad") == broken);
    assert(nlohmann::json::accept(ReadAll(settings)));  // the new file is complete JSON
}

static void TestBrokenIconsAreKept(const fs::path& pluginDir) {
    const auto icons = pluginDir / "config" / "icons.json";
    const std::string broken = "{ \"BaseIcons\": {";
    WriteRaw(icons, broken);

    IconTables tables;
    tables.Load(pluginDir);
    tables.Save(pluginDir);

    assert(ReadAll(pluginDir / "config" / "icons.json.bad") == broken);
}

static size_t IndexOf(const std::vector<DisplayRule>& rules, std::string_view id) {
    for (size_t i = 0; i < rules.size(); ++i)
        if (rules[i].id == id) return i;
    return SIZE_MAX;
}

static const char* const kUniqueBoxRules[] = {"stock.vaal_strongbox", "stock.ezomyte_strongbox",
                                              "stock.ventors_strongbox", "stock.etched_beetle_strongbox"};

// Unique strongboxes share a normal box's path, so their stock rules must come first and match on Model.
static void TestUniqueStrongboxDefaults() {
    const auto rules = IconTables::DefaultDisplayRules();
    for (const char* id : kUniqueBoxRules) {
        const size_t i = IndexOf(rules, id);
        assert(i != SIZE_MAX);
        assert(!rules[i].models.empty());
        assert(i < IndexOf(rules, "stock.researchers_strongbox"));
        assert(i < IndexOf(rules, "stock.strongbox_metadata"));
    }
    const auto& vaal = rules[IndexOf(rules, "stock.vaal_strongbox")];
    assert(vaal.models[0] == "VaalStrongbox");
    const auto back = IconTables::ParseDisplayRule(IconTables::WriteDisplayRule(vaal));
    assert(back.models == vaal.models);
}

static void WriteRulesFile(const fs::path& pluginDir, const std::vector<DisplayRule>& rules, int schema) {
    nlohmann::json j;
    j["SchemaVersion"] = schema;
    j["DisplayRules"] = nlohmann::json::array();
    for (const auto& r : rules) j["DisplayRules"].push_back(IconTables::WriteDisplayRule(r));
    WriteRaw(DisplayRulesStore::PathFor(pluginDir), j.dump(4));
}

static std::vector<std::string> StockIds(const std::vector<DisplayRule>& rules) {
    std::vector<std::string> ids;
    for (const auto& r : rules)
        if (r.id.rfind("stock.", 0) == 0) ids.push_back(r.id);
    return ids;
}

// Rules are first-match: a specific rule behind a catch-all never runs.
static void TestStockRuleOrder() {
    const auto rules = IconTables::DefaultDisplayRules();
    for (const auto& r : rules)
        for (const auto& term : r.matchTerms) assert(term.find('\\') == std::string::npos);  // game paths use '/'
    auto before = [&](const char* a, const char* b) { return IndexOf(rules, a) < IndexOf(rules, b); };
    for (const char* id : {"stock.legacy_of_the_order", "stock.researchers_strongbox", "stock.blacksmiths_strongbox",
                           "stock.cartographers_strongbox", "stock.corruption_strongbox", "stock.strongbox_large"})
        assert(before(id, "stock.strongbox_metadata"));
    for (const char* id : {"stock.omen_chest", "stock.breach_chest", "stock.expedition_chest", "stock.abyssal_strongbox"})
        assert(before(id, "stock.chest_generic"));
    assert(before("stock.expedition", "stock.delirium_bomb"));  // the size-0 catch-all hides everything after it
}

// Chest paths from the PoE2 0.5.5.10 Chests table and the rule each should reach. Only rules that
// decide on path alone take part, which is how these chests are told apart.
static void TestChestPathsReachTheRightRule() {
    const auto rules = IconTables::DefaultDisplayRules();
    auto firstPathRule = [&](const std::string& path, const char* category = "Chest") -> std::string {
        for (const auto& r : rules) {
            if (!r.enabled || r.hide || r.categories.empty()
                || std::find(r.categories.begin(), r.categories.end(), category) == r.categories.end())
                continue;
            if (!r.models.empty() || !r.mods.empty() || !r.subtypes.empty() || !r.states.empty() || !r.rarity.empty())
                continue;
            if (r.matchTerms.empty()) return r.id;  // a catch-all (the hide rules come first and are skipped)
            for (const auto& term : r.matchTerms)
                if (MatchesRuleTerm(path, term)) return r.id;
        }
        return "";
    };
    const std::pair<const char*, const char*> cases[] = {
        {"StrongBoxes/ResearchStrongboxLow", "stock.researchers_strongbox"},
        {"StrongBoxes/ResearchStrongbox", "stock.researchers_strongbox"},
        {"StrongBoxes/JewellerStrongboxExpedition", "stock.jewellers_strongbox"},
        {"StrongBoxes/CasterStrongboxHigh", "stock.arcane_strongbox"},
        {"StrongBoxes/ArmourerStrongboxLow", "stock.armourer_strongbox"},
        {"StrongBoxes/OrnateStrongbox", "stock.ornate_strongbox"},
        {"StrongBoxes/MartialStrongbox", "stock.blacksmiths_strongbox"},
        {"StrongBoxes/MapStrongboxExpedition", "stock.cartographers_strongbox"},
        {"StrongBoxes/LargeStrongboxHigh", "stock.strongbox_large"},
        {"StrongBoxes/GemStrongbox", "stock.corruption_strongbox"},
        {"StrongBoxes/BasicStrongbox", "stock.strongbox_metadata"},
        {"StrongBoxes/LandmarkStrongbox/P1_4LandmarkStrongbox1", "stock.strongbox_metadata"},
        {"Abyss/AbyssStrongboxWeapons", "stock.abyssal_strongbox"},
        {"LeagueAzmeri/OmenChest", "stock.omen_chest"},
        {"LeagueAzmeri/LandmarkOmenChest", "stock.omen_chest"},
        {"Barrels/Barrel1", "stock.chest_generic"},
    };
    for (const auto& [path, want] : cases) {
        const auto got = firstPathRule(std::string("Metadata/Chests/") + path);
        if (got != want) std::fprintf(stderr, "%s -> %s (want %s)\n", path, got.c_str(), want);
        assert(got == want);
    }
    // Azmeri wisp deposits (MiscObjects AzmeriDustCloud): one path for every kind, shown as an
    // object instead of falling into the size-0 catch-all that hides other objects.
    assert(firstPathRule("Metadata/MiscellaneousObjects/Azmeri/AzmeriResourceBase", "Object") == "stock.azmeri_wisps");
    assert(firstPathRule("Metadata/MiscellaneousObjects/Azmeri/AzmeriResourcePrimalistBig", "Object") == "stock.azmeri_wisps");
    assert(firstPathRule("Metadata/MiscellaneousObjects/Azmeri/AzmeriFuelResupply", "Object") == "stock.wisp_fuel");
    assert(firstPathRule("Metadata/MiscellaneousObjects/Azmeri/AzmeriLightBomb", "Object") == "stock.wisp_light");
    assert(firstPathRule("Metadata/MiscellaneousObjects/SomethingElse", "Object") == "stock.delirium_bomb");
}

// An old file gets the later stock changes once and in place, without touching the user's edits.
static void TestDisplayRulesUpgrade(const fs::path& pluginDir) {
    const auto defaults = IconTables::DefaultDisplayRules();
    auto old = defaults;  // roughly the schema-1 defaults
    for (const char* id : {"stock.vaal_strongbox", "stock.ezomyte_strongbox", "stock.ventors_strongbox",
                           "stock.etched_beetle_strongbox", "stock.legacy_of_the_order", "stock.blacksmiths_strongbox",
                           "stock.cartographers_strongbox", "stock.corruption_strongbox", "stock.abyssal_strongbox",
                           "stock.azmeri_wisps", "stock.wisp_fuel", "stock.possessed_vivid", "stock.free_wisp_sacred"})
        old.erase(old.begin() + IndexOf(old, id));
    old[IndexOf(old, "stock.researchers_strongbox")].matchTerms = {"StrongBoxes/ResearchStrongboxHigh"};
    old[IndexOf(old, "stock.researchers_strongbox")].markerColor = {1, 2, 3, 255};  // user edit
    old[IndexOf(old, "stock.omen_chest")].matchTerms = {"Chests\\LeagueAzmeri\\OmenChest"};
    for (const char* id : {"stock.strongbox_large", "stock.breach_chest", "stock.expedition_chest", "stock.expedition"}) {
        const auto rule = old[IndexOf(old, id)];  // where the old defaults had them
        old.erase(old.begin() + IndexOf(old, id));
        old.push_back(rule);
    }
    DisplayRule mine;
    mine.id = "user.my_rule";
    mine.source = "User";
    mine.name = "Mine";
    old.insert(old.begin(), mine);
    WriteRulesFile(pluginDir, old, 1);

    std::vector<DisplayRule> rules;
    DisplayRulesStore::Load(pluginDir, rules);
    assert(StockIds(rules) == StockIds(defaults));  // same order as a fresh install
    const auto& researchers = rules[IndexOf(rules, "stock.researchers_strongbox")];
    assert(researchers.matchTerms == defaults[IndexOf(defaults, "stock.researchers_strongbox")].matchTerms);
    assert(researchers.markerColor.r == 1);
    assert(rules[IndexOf(rules, "stock.omen_chest")].matchTerms == std::vector<std::string>{"OmenChest"});
    assert(IndexOf(rules, "user.my_rule") == 0);
    assert(nlohmann::json::parse(ReadAll(DisplayRulesStore::PathFor(pluginDir)))["SchemaVersion"] == DisplayRulesStore::kSchemaVersion);

    // Deleting a stock rule afterwards sticks.
    rules.erase(rules.begin() + IndexOf(rules, "stock.vaal_strongbox"));
    std::string note;
    assert(DisplayRulesStore::Save(pluginDir, rules, note));
    std::vector<DisplayRule> again;
    DisplayRulesStore::Load(pluginDir, again);
    assert(IndexOf(again, "stock.vaal_strongbox") == SIZE_MAX);

    // A schema-2 file (from the first unique-box build) gets only the schema-3 changes.
    auto v2 = defaults;
    v2.erase(v2.begin() + IndexOf(v2, "stock.ezomyte_strongbox"));  // deleted by the user under schema 2
    v2.erase(v2.begin() + IndexOf(v2, "stock.abyssal_strongbox"));
    v2.erase(v2.begin() + IndexOf(v2, "stock.azmeri_wisps"));
    WriteRulesFile(pluginDir, v2, 2);
    std::vector<DisplayRule> fromV2;
    DisplayRulesStore::Load(pluginDir, fromV2);
    assert(IndexOf(fromV2, "stock.ezomyte_strongbox") == SIZE_MAX);
    assert(IndexOf(fromV2, "stock.abyssal_strongbox") != SIZE_MAX);
    assert(IndexOf(fromV2, "stock.azmeri_wisps") != SIZE_MAX);

    // A schema-4 file (the first wisp build) gets the wider wisp path, keeping its colour.
    auto v4 = defaults;
    auto& wild = v4[IndexOf(v4, "stock.wisps_wild")];
    wild.matchTerms = {"MiscellaneousObjects/Azmeri/AzmeriResourceBase"};
    wild.markerColor = {9, 9, 9, 255};
    WriteRulesFile(pluginDir, v4, 4);
    std::vector<DisplayRule> fromV4;
    DisplayRulesStore::Load(pluginDir, fromV4);
    const auto& wildNow = fromV4[IndexOf(fromV4, "stock.wisps_wild")];
    assert(wildNow.matchTerms == std::vector<std::string>{"MiscellaneousObjects/Azmeri/AzmeriResource"});
    assert(wildNow.markerColor.r == 9);
}

// Stand-in for PluginSDK::AreaRoom (the SDK header needs Windows).
struct FakeRoom {
    std::string Name;
    float GridX = 0, GridY = 0;
    bool Matches(const std::string& pattern) const { return Name.find(pattern) != std::string::npos; }
};

static void TestSelectRoomTargets() {
    const std::vector<FakeRoom> rooms = {
        {"Rooms/Breach_01.arm", 100, 0}, {"Rooms/Ritual_02.arm", 5, 0},
        {"Rooms/Breach_02.arm", 10, 0},  {"Rooms/Breach_03.arm", 50, 0}};
    assert(SelectRoomTargets(rooms, "Breach", 0, 0, 0).size() == 3);  // 0 = every match
    const auto nearest = SelectRoomTargets(rooms, "Breach", 2, 0, 0);
    assert(nearest.size() == 2 && nearest[0]->GridX == 10 && nearest[1]->GridX == 50);
    assert(SelectRoomTargets(rooms, "Breach", 9, 0, 0).size() == 3);  // more than exist
    assert(SelectRoomTargets(rooms, "Delirium", 0, 0, 0).empty());
}

static RoomTarget MakeRoom(const char* name, const char* pattern, const char* category, const char* area) {
    RoomTarget r;
    r.pattern = pattern;
    r.category = category;
    r.area = area;
    r.look.name = name;
    r.look.expectedCount = 0;
    return r;
}

static std::vector<std::string> Names(const std::vector<const RoomTarget*>& targets) {
    std::vector<std::string> out;
    for (const auto* t : targets) out.push_back(t->look.name);
    std::sort(out.begin(), out.end());
    return out;
}

// rooms.json round trip, and which targets an area gets: its own plus its category's globals.
static void TestRoomTargetStore(const fs::path& pluginDir) {
    {
        RoomTargetStore store;
        store.targets = {MakeRoom("Breach", "*breach*", "Acts", "*"), MakeRoom("Ritual", "*ritual*", "Acts", "G1_1"),
                         MakeRoom("Shrine", "*shrine*", "Acts", "G1_1"), MakeRoom("Boss", "*boss*", "Endgame", "*")};
        store.targets.back().look.iconSize = 42.f;
        auto off = MakeRoom("Off", "*off*", "Acts", "*");
        off.look.enabled = false;
        store.targets.push_back(off);
        assert(store.Save(pluginDir));
    }
    RoomTargetStore store;
    store.Load(pluginDir);
    assert(store.targets.size() == 5);
    assert(store.targets[3].look.iconSize == 42.f && store.targets[3].pattern == "*boss*");
    assert((Names(store.ForArea("G1_1", false)) == std::vector<std::string>{"Breach", "Ritual", "Shrine"}));
    assert((Names(store.ForArea("G2_3", false)) == std::vector<std::string>{"Breach"}));
    assert((Names(store.ForArea("MAPBLUFF", true)) == std::vector<std::string>{"Boss"}));

    WriteRaw(RoomTargetStore::PathFor(pluginDir), "{ \"RoomTargets\": [");  // broken file is kept
    store.Load(pluginDir);
    assert(store.targets.empty());
    assert(fs::exists(fs::path(RoomTargetStore::PathFor(pluginDir)) += ".bad"));
}

// The first room-target build stored them in user.json with RoomPattern; they move to rooms.json.
static void TestRoomTargetsMoveOutOfUserJson(const fs::path& pluginDir) {
    WriteRaw(pluginDir / "config" / "targets" / "user.json", R"({
        "G1_1": [{"Name": "Spot", "Path": "Metadata/Spot"},
                 {"Name": "Ritual", "Path": "", "RoomPattern": "*ritual*", "ExpectedCount": 2}],
        "MAPBLUFF": [{"Name": "Boss", "Path": "", "RoomPattern": "*boss*"}]
    })");
    TargetDatabase db;
    db.Load(pluginDir);
    for (const auto* t : db.GetTargetsForArea("G1_1", "G1_1")) assert(t->name != "Ritual");  // not a landmark
    RoomTargetStore store;
    assert(store.Adopt(db) == 2);
    assert(store.targets[0].pattern == "*ritual*" && store.targets[0].area == "G1_1");
    assert(store.targets[0].category == "Acts" && store.targets[0].look.expectedCount == 2);
    assert(store.targets[1].category == "Endgame");
    assert(db.SaveUser(pluginDir));
    const auto user = ReadAll(pluginDir / "config" / "targets" / "user.json");
    assert(user.find("RoomPattern") == std::string::npos && user.find("Metadata/Spot") != std::string::npos);
}

// The Ignore tab edits ignore.json: names and switches survive a save, and disabled entries stop matching.
static void TestIgnoreList(const fs::path& pluginDir) {
    const auto file = pluginDir / "config" / "targets" / "ignore.json";
    const char* armoury = "Metadata/Chests/LeagueIncursion/ArmouryTier3";
    {
        TargetDatabase db;
        db.Load(pluginDir);  // no file yet: the bundled list
        assert(db.ignoreEntries.size() == 7);
        assert(db.ignorePatterns.MatchesAny(armoury));
        db.ignoreEntries[0].enabled = false;  // the Armoury entry
        db.RebuildIgnorePatterns();
        assert(!db.ignorePatterns.MatchesAny(armoury));
        db.AddIgnore("Barrels", "Metadata/Chests/Barrels*");
        assert(db.ignorePatterns.MatchesAny("Metadata/Chests/Barrels/Barrel1"));
        assert(db.SaveIgnore(pluginDir));
    }
    TargetDatabase db;
    db.Load(pluginDir);
    assert(db.ignoreEntries.size() == 8 && !db.ignoreEntries[0].enabled);
    assert(db.ignoreEntries[7].name == "Barrels");
    assert(!db.ignorePatterns.MatchesAny(armoury));
    assert(db.ignorePatterns.MatchesAny("Metadata/Chests/Barrels/Barrel1"));

    WriteRaw(file, "{ \"*\": [");  // broken: kept, and the bundled list is used meanwhile
    db.Load(pluginDir);
    assert(fs::exists(fs::path(file) += ".bad"));
    assert(db.ignoreEntries.size() == 7);
}

// A user landmark saved for all areas shows everywhere and survives saving twice.
static void TestGlobalUserLandmark(const fs::path& pluginDir) {
    TargetEntry t;
    t.name = "Everywhere";
    t.path = "Metadata/Spot*";
    {
        TargetDatabase db;
        db.Load(pluginDir);
        db.UpsertUserTarget("*", t);
        assert(db.SaveUser(pluginDir));
    }
    for (int round = 0; round < 2; ++round) {
        TargetDatabase db;
        db.Load(pluginDir);
        for (const char* area : {"G1_1", "MAPBLUFF"}) {
            bool found = false;
            for (const auto* e : db.GetTargetsForArea(area, area)) found |= e->name == "Everywhere";
            assert(found);
        }
        assert(db.DisplayNameForArea("*") == "Global");
        assert(db.SaveUser(pluginDir));
    }
}

// Wisp models from AzmeriWoodsDustType.WispAnimations (0.5.5.10): each reaches exactly its kind
// rule, ahead of the Unknown fallback, and the size part sets the marker scale.
static void TestWispKinds() {
    const auto rules = IconTables::DefaultDisplayRules();
    const std::pair<const char*, const char*> kinds[] = {
        {"warden", "stock.wisps_vivid"}, {"primal", "stock.wisps_primal"},
        {"vodoo", "stock.wisps_wild"}, {"sacred", "stock.wisps_sacred"}};
    for (const auto& [kind, id] : kinds) {
        assert(IndexOf(rules, id) < IndexOf(rules, "stock.azmeri_wisps"));
        for (const char* size : {"sml", "med", "big"}) {
            const std::string model = std::string("Metadata/Effects/Spells/monsters_effects/League_Azmeri/resources/"
                                                  "wisp_doodads/wisp_") + kind + "_" + size + ".ao";
            std::string first;
            for (const auto& r : rules)
                if (!r.models.empty() && ContainsCaseInsensitiveRuleText(model, r.models[0]) && first.empty()) first = r.id;
            assert(first == id);
        }
    }
    assert(WispSizeScale("wisp_primal_big.ao") == 1.f);
    assert(WispSizeScale("wisp_primal_med.ao") == 0.85f);
    assert(WispSizeScale("wisp_primal_sml.ao") == 0.70f);
    assert(WispSizeScale("Metadata/MiscellaneousObjects/Azmeri/AzmeriResourceBase") == 1.f);
}

// First run gets the Wildwood feature rooms; every room file of AzmeriFeatureRooms matches one.
static void TestWildwoodRoomTargets(const fs::path& pluginDir) {
    // The Wildwood is MapUniqueWildwood, level 65, IsMapArea (WorldAreas 0.5.5): Endgame by id or by level.
    TargetDatabase db;
    assert(db.IsEndgameArea("MapUniqueWildwood"));
    assert(db.IsEndgameArea("The Viridian Wildwood", 65));
    assert(!db.IsEndgameArea("G1_1", 12));

    RoomTargetStore store;
    store.Load(pluginDir);  // no rooms.json yet
    assert(store.targets.size() == 14);
    assert(store.ForArea("MAPUNIQUEWILDWOOD", true).size() == 14 && store.ForArea("G1_1", false).empty());
    for (const char* file : {"clearing_ambush_01.arm","clearing_dustpile_02.arm","clearing_gruthkul_blood_01_01.arm","clearing_gruthkul_blood_01_02.arm","clearing_gruthkul_blood_01_03.arm","clearing_gruthkul_blood_01_04.arm","clearing_gruthkul_blood_02_01.arm","clearing_ambush_02.arm","clearing_dustpile_01.arm","clearing_treant_henge_01_01.arm","clearing_treant_henge_01_02.arm","clearing_treant_henge_01_03.arm","clearing_treant_henge_01_04.arm","clearing_treant_henge_02_01.arm","clearing_ambush_03.arm","clearing_dustpile_03.arm","clearing_huts_01_01.arm","clearing_huts_01_02.arm","clearing_huts_01_03.arm","clearing_huts_01_04.arm","clearing_huts_02_02.arm","clearing_huts_02_03.arm","clearing_huts_02_01.arm","clearing_huts_03_01.arm","clearing_huts_03_04.arm","clearing_wolves_01.arm","shrine_gruthkul.arm","clearing_spidernest_01.arm","clearing_saplings_01.arm","shrine_ryslatha.arm","clearing_trout_swamp_01.arm","shrine_lunaris.arm","shrine_sisters.arm","shrine_solaris.arm"}) {
        const std::string name = std::string("Metadata/Terrain/Maps/UniqueWildwood/Rooms/Features/") + file;
        int matches = 0;
        for (const auto& r : store.targets)  // every default pattern is "<prefix>*"
            matches += name.rfind(r.pattern.substr(0, r.pattern.size() - 1), 0) == 0;
        assert(matches == 1);
    }
    assert(store.Missing(RoomTargetStore::WildwoodTargets()).empty());
    store.targets.erase(store.targets.begin());
    assert(store.Missing(RoomTargetStore::WildwoodTargets()).size() == 1);
    assert(store.Save(pluginDir));
    RoomTargetStore again;  // an existing file is never re-seeded
    again.Load(pluginDir);
    assert(again.targets.size() == 13);
}

// Azmeri spirits from PoE2 0.5.5.10: free wisps by MonsterVarieties path, possessed and touched
// monsters by mod (SpiritPossessed<Attr> -> BuffTemplates TormentedSpiritPower<Tier>).
static void TestAzmeriSpirits() {
    const auto rules = IconTables::DefaultDisplayRules();
    auto firstMonsterRule = [&](const std::string& path, const std::string& modId) -> std::string {
        for (const auto& r : rules) {
            if (!r.enabled || std::find(r.categories.begin(), r.categories.end(), "Monster") == r.categories.end()) continue;
            if (!r.subtypes.empty() || !r.states.empty() || !r.rarity.empty() || r.life == "Dead") continue;
            bool ok = r.matchTerms.empty();
            for (const auto& t : r.matchTerms) ok |= MatchesRuleTerm(path, t);
            if (!r.mods.empty()) {
                bool mod = false;
                for (const auto& t : r.mods) mod |= !modId.empty() && MatchesRuleTerm(modId, t);
                ok &= mod;
            }
            if (ok) return r.id;
        }
        return "";
    };
    const std::string spirits = "Metadata/Monsters/TormentedSpirits/";
    const std::pair<const char*, const char*> free[] = {
        {"TormentedSpiritoftheWolfVivid", "stock.free_wisp_vivid"}, {"TormentedSpiritoftheStagVividViper", "stock.free_wisp_vivid"},
        {"TormentedSpiritoftheOwlPrimal", "stock.free_wisp_primal"}, {"TormentedSpiritoftheBearWildViper", "stock.free_wisp_wild"},
        {"TormentedSpiritoftheRabbitSacred_", "stock.free_wisp_sacred"}, {"TormentedSpiritoftheFoxSacred", "stock.free_wisp_sacred"},
        {"TormentedSpiritoftheOwlPrimalPossesedDaemon", "stock.hide_wisp_daemon"},
        {"TormentedSpiritoftheRabbitSacredPossesedDaemonPlayer", "stock.hide_wisp_daemon"}};
    for (const auto& [name, want] : free) {
        const auto got = firstMonsterRule(spirits + name, "");
        if (got != want) std::fprintf(stderr, "%s -> %s (want %s)\n", name, got.c_str(), want);
        assert(got == want);
    }
    const std::pair<const char*, const char*> mods[] = {
        {"SpiritPossessedDex", "stock.possessed_vivid"}, {"SpiritPossessedInt", "stock.possessed_primal"},
        {"SpiritPossessedStr", "stock.possessed_wild"}, {"SpiritPossessedSacred", "stock.possessed_sacred"},
        {"SpiritTouchedDex", "stock.spirit_touched"}, {"SpiritTouchedSacred", "stock.spirit_touched"}};
    for (const auto& [mod, want] : mods) assert(firstMonsterRule("Metadata/Monsters/Wolves/Wolf", mod) == want);
    // An ordinary monster reaches none of the spirit rules.
    const auto plain = firstMonsterRule("Metadata/Monsters/Wolves/Wolf", "SomeOtherMod");
    for (const char* prefix : {"stock.possessed_", "stock.spirit_touched", "stock.free_wisp_", "stock.hide_wisp_daemon"})
        assert(plain.rfind(prefix, 0) != 0);
    assert(WispKindIndex(".../wisp_doodads/wisp_vodoo_big.ao") == 2 && WispKindIndex("") == 4);
}

// A grid from text: '#' wall, digits = cell value (walkable), '.' = 5 (open ground).
static RadarPath::Grid GridFrom(std::initializer_list<const char*> rows) {
    RadarPath::Grid g;
    g.height = static_cast<int>(rows.size());
    for (const char* r : rows) {
        g.width = static_cast<int>(std::strlen(r));
        for (int x = 0; x < g.width; ++x) {
            const uint8_t v = r[x] == '#' ? 0 : r[x] == '.' ? 5 : static_cast<uint8_t>(r[x] - '0');
            g.cells.push_back(v);
            g.maxValue = std::max(g.maxValue, v);
        }
    }
    return g;
}

static void TestPathField() {
    using namespace RadarPath;
    // 4 bits per cell, low nibble first (PoE2 walkable grid).
    const uint8_t packed[] = {0x21, 0x03};
    const Grid d = DecodeWalkable(packed, sizeof(packed), 4, 1);
    assert((d.cells == std::vector<uint8_t>{1, 2, 3, 0}) && d.maxValue == 3);

    // The only way through the wall is the gap at the bottom.
    const Grid g = GridFrom({".....#.....",
                             ".....#.....",
                             ".....#.....",
                             "..........."});
    DistanceField f;
    f.Build(g, {0, 0});
    const auto path = TracePath(g, f, {10, 0});
    assert(path.size() >= 3 && path.front() == (Cell{0, 0}) && path.back() == (Cell{10, 0}));
    for (size_t i = 0; i + 1 < path.size(); ++i) assert(HasClearLine(g, path[i], path[i + 1]));
    bool viaGap = false;
    for (const auto& c : path) viaGap |= c.y == 3;
    assert(viaGap);

    // No squeezing diagonally between two wall corners.
    const Grid corner = GridFrom({".#",
                                  "#."});
    DistanceField fc;
    fc.Build(corner, {0, 0});
    assert(fc.CostAt(1, 1) == DistanceField::kUnreached && TracePath(corner, fc, {1, 1}).empty());
    assert(!HasClearLine(corner, {0, 0}, {1, 1}));

    // Same length both ways: the open lane (5) wins over the one along a wall (1).
    const Grid lanes = GridFrom({"1111111",
                                 "1#####1",
                                 ".......",});
    DistanceField fl;
    fl.Build(lanes, {0, 1});
    const auto lanePath = TracePath(lanes, fl, {6, 1}, 1);  // segment 1: keep every cell
    bool open = false;
    for (const auto& c : lanePath) open |= c.y == 2;
    assert(open);

    // Stopping once the target is final gives the full field's cost.
    DistanceField early;
    const std::vector<Cell> targets{{10, 0}};
    early.Build(g, {0, 0}, &targets);
    assert(early.CostAt(10, 0) == f.CostAt(10, 0));
    assert(TracePath(g, early, {10, 0}).back() == (Cell{10, 0}));

    Cell near;
    assert(NearestWalkable(g, 5, 1, 2, near) && g.Walkable(near.x, near.y) && std::abs(near.x - 5) == 1);
}

// The worker: a request gives a path, a new area drops the old paths, and a path traced on the
// worker matches the one traced directly.
static void TestPathService() {
    using namespace RadarPath;
    const Grid g = GridFrom({".....#.....",
                             ".....#.....",
                             "..........."});
    std::vector<uint8_t> packed((static_cast<size_t>(g.width) + 1) / 2 * g.height, 0);
    for (int y = 0; y < g.height; ++y)
        for (int x = 0; x < g.width; ++x)
            packed[static_cast<size_t>(y) * ((g.width + 1) / 2) + x / 2] |= static_cast<uint8_t>(g.At(x, y) << ((x & 1) * 4));
    auto waitFor = [](PathService& s, uint64_t gridId) {
        for (int i = 0; i < 400; ++i) {
            if (auto r = s.Latest(); r && r->gridId == gridId && !s.Busy()) return r;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return std::shared_ptr<const PathResult>{};
    };
    PathService service;
    service.SetGrid(packed, g.width, g.height, 1);
    service.Request({0, 0}, {{7, {10, 0}}, {8, {5, 0}}});  // (5,0) is a wall: snapped next to it
    auto r = waitFor(service, 1);
    assert(r && r->paths.size() == 2);
    DistanceField f;
    f.Build(g, {0, 0});
    assert(r->paths[0].key == 7 && r->paths[0].points == TracePath(g, f, {10, 0}));
    assert(g.Walkable(r->paths[1].points.back().x, r->paths[1].points.back().y));

    service.SetGrid(packed, g.width, g.height, 2);  // a new area: no paths until asked again
    assert(!service.Latest());
    service.Request({0, 0}, {{7, {10, 0}}});
    r = waitFor(service, 2);
    assert(r && r->paths.size() == 1);

    // When DrawUI sends a request.
    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    RequestGate gate;
    const std::vector<PathTarget> one{{1, {100, 100}}};
    assert(gate.ShouldSend({0, 0}, one, 1, t0));                                       // first
    assert(!gate.ShouldSend({10, 0}, one, 1, t0 + std::chrono::seconds(5)));           // moved a little
    assert(!gate.ShouldSend({40, 0}, one, 1, t0 + std::chrono::milliseconds(100)));    // far, too soon
    assert(gate.ShouldSend({40, 0}, one, 1, t0 + std::chrono::milliseconds(600)));     // far, later
    assert(gate.ShouldSend({40, 0}, {{2, {100, 100}}}, 1, t0 + std::chrono::milliseconds(601)));  // new target
    assert(gate.ShouldSend({40, 0}, {{2, {100, 100}}}, 2, t0 + std::chrono::milliseconds(602)));  // new area
}

// Draw Path is kept for your own landmarks, for bundled ones (as an override) and for room targets.
static void TestDrawPathSaved(const fs::path& pluginDir) {
    auto countBundled = [](const TargetDatabase& db) {  // the shipped files already set DrawPath on most
        int n = 0;
        for (const auto& t : db.storage) n += t.category != "User" && t.drawPath;
        return n;
    };
    int bundledBefore = 0;
    {
        TargetDatabase db;
        db.Load(pluginDir);
        bundledBefore = countBundled(db);
        TargetEntry mine;
        mine.name = "Mine";
        mine.path = "Metadata/Mine*";
        mine.drawPath = true;
        db.UpsertUserTarget("G1_1", mine);
        assert(db.SaveUser(pluginDir));
        size_t bundled = SIZE_MAX;  // the first bundled landmark
        for (size_t i = 0; i < db.storage.size() && bundled == SIZE_MAX; ++i)
            if (db.storage[i].category != "User" && !db.storage[i].drawPath) bundled = i;
        assert(bundled != SIZE_MAX);
        db.storage[bundled].drawPath = true;
        assert(db.SaveBundledOverride(bundled, pluginDir));
    }
    TargetDatabase db;
    db.Load(pluginDir);
    int user = 0;
    for (const auto& t : db.storage) user += t.category == "User" && t.drawPath;
    assert(user == 1 && countBundled(db) == bundledBefore + 1);

    RoomTargetStore rooms;
    rooms.targets = {MakeRoom("Arena", "*arena*", "Endgame", "*")};
    rooms.targets[0].look.drawPath = true;
    assert(rooms.Save(pluginDir));
    RoomTargetStore again;
    again.Load(pluginDir);
    assert(again.targets.size() == 1 && again.targets[0].look.drawPath);
}

int main() {
    const auto dir = fs::temp_directory_path() / "altradar_self_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    TestPathMatcher();
    TestWriteFileAtomic(dir);
    TestBrokenUserTargetsAreKept(dir / "plugin1");
    TestBrokenSettingsAreKept(dir / "plugin2");
    TestBrokenIconsAreKept(dir / "plugin3");
    TestUniqueStrongboxDefaults();
    TestStockRuleOrder();
    TestChestPathsReachTheRightRule();
    TestDisplayRulesUpgrade(dir / "plugin4");
    TestSelectRoomTargets();
    TestRoomTargetStore(dir / "plugin5");
    TestRoomTargetsMoveOutOfUserJson(dir / "plugin6");
    TestIgnoreList(dir / "plugin7");
    TestGlobalUserLandmark(dir / "plugin8");
    TestWispKinds();
    TestAzmeriSpirits();
    TestPathField();
    TestPathService();
    TestDrawPathSaved(dir / "plugin10");
    TestWildwoodRoomTargets(dir / "plugin9");

    fs::remove_all(dir);
    std::puts("AltRadar self-test: all checks passed");
    return 0;
}
