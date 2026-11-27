// Self-checks for AltRadar's file and pattern logic. No game or host needed.
// Build and run from the repo root (CI does this before every build):
//   g++ -std=c++20 -IPlugins/AltRadar -IPOEFixer/imgui Plugins/AltRadar/tests/self_test.cpp -o self_test && ./self_test
#include <ctime>
#ifndef _WIN32
// Map the MSVC-only functions the data code uses, so this also runs on Linux.
#include <cstdio>
#define localtime_s(tmOut, timeIn) localtime_r(timeIn, tmOut)
#define sscanf_s sscanf  // only used with %d, where the two behave the same
#endif

#undef NDEBUG
#include "data/DisplayRulesStore.h"
#include "data/IconTables.h"
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

// A schema-1 file gets the new stock rules once, in place, without touching the user's edits.
static void TestDisplayRulesUpgrade(const fs::path& pluginDir) {
    auto old = IconTables::DefaultDisplayRules();
    for (const char* id : kUniqueBoxRules) old.erase(old.begin() + IndexOf(old, id));
    old[IndexOf(old, "stock.researchers_strongbox")].markerColor = {1, 2, 3, 255};  // user edit
    DisplayRule mine;
    mine.id = "user.my_rule";
    mine.source = "User";
    mine.name = "Mine";
    old.insert(old.begin(), mine);
    WriteRulesFile(pluginDir, old, 1);

    std::vector<DisplayRule> rules;
    DisplayRulesStore::Load(pluginDir, rules);
    const size_t researchers = IndexOf(rules, "stock.researchers_strongbox");
    for (size_t k = 0; k < 4; ++k) assert(IndexOf(rules, kUniqueBoxRules[k]) == researchers - 4 + k);
    assert(rules[researchers].markerColor.r == 1);
    assert(IndexOf(rules, "user.my_rule") != SIZE_MAX);
    assert(nlohmann::json::parse(ReadAll(DisplayRulesStore::PathFor(pluginDir)))["SchemaVersion"] == 2);

    // Deleting a new rule afterwards sticks.
    rules.erase(rules.begin() + IndexOf(rules, "stock.vaal_strongbox"));
    std::string note;
    assert(DisplayRulesStore::Save(pluginDir, rules, note));
    std::vector<DisplayRule> again;
    DisplayRulesStore::Load(pluginDir, again);
    assert(IndexOf(again, "stock.vaal_strongbox") == SIZE_MAX);
    assert(IndexOf(again, "stock.ezomyte_strongbox") != SIZE_MAX);
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

static TargetEntry RoomTarget(const char* name, const char* pattern) {
    TargetEntry t;
    t.name = name;
    t.roomPattern = pattern;
    t.expectedCount = 0;
    return t;
}

// Room targets for one area or all areas survive a save, and two in one area stay separate.
static void TestRoomTargetsPersist(const fs::path& pluginDir) {
    {
        TargetDatabase db;
        db.Load(pluginDir);
        db.UpsertUserTarget("*", RoomTarget("Any breach", "*breach*"));
        db.UpsertUserTarget("G1_1", RoomTarget("Ritual", "*ritual*"));
        db.UpsertUserTarget("G1_1", RoomTarget("Shrine", "*shrine*"));
        assert(db.SaveUser(pluginDir));
    }
    TargetDatabase db;
    db.Load(pluginDir);
    auto names = [&](const char* area) {
        std::vector<std::string> out;
        for (const auto* t : db.GetTargetsForArea(area, area))
            if (!t->roomPattern.empty()) out.push_back(t->name + "=" + t->roomPattern);
        std::sort(out.begin(), out.end());
        return out;
    };
    assert((names("G1_1") == std::vector<std::string>{"Any breach=*breach*", "Ritual=*ritual*", "Shrine=*shrine*"}));
    assert((names("G2_3") == std::vector<std::string>{"Any breach=*breach*"}));
    assert(db.DisplayNameForArea("*") == "Global");

    // A second save must keep the all-areas target too (it used to load where SaveUser never looks).
    assert(db.SaveUser(pluginDir));
    TargetDatabase again;
    again.Load(pluginDir);
    bool kept = false;
    for (const auto* t : again.GetTargetsForArea("G2_3", "G2_3")) kept |= t->roomPattern == "*breach*";
    assert(kept);
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
    TestDisplayRulesUpgrade(dir / "plugin4");
    TestSelectRoomTargets();
    TestRoomTargetsPersist(dir / "plugin5");

    fs::remove_all(dir);
    std::puts("AltRadar self-test: all checks passed");
    return 0;
}
