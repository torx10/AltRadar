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
#include "data/IconTables.h"
#include "data/PathMatcher.h"
#include "data/RadarConfig.h"
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

int main() {
    const auto dir = fs::temp_directory_path() / "altradar_self_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    TestPathMatcher();
    TestWriteFileAtomic(dir);
    TestBrokenUserTargetsAreKept(dir / "plugin1");
    TestBrokenSettingsAreKept(dir / "plugin2");
    TestBrokenIconsAreKept(dir / "plugin3");

    fs::remove_all(dir);
    std::puts("AltRadar self-test: all checks passed");
    return 0;
}
