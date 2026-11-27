#pragma once

#include "FileIo.h"
#include "PathMatcher.h"
#include "RadarLog.h"
#include "TargetDatabase.h"

#include "../third_party/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace RadarData {

// A marker at the centre of every room of the area layout whose name matches `pattern` (the
// SDK's AreaRoom rule). Rooms are known as soon as an area loads, before anything in them is
// in range. Kept apart from landmarks, which match tiles and entities.
struct RoomTarget {
    std::string pattern;
    std::string category = "Acts";  // "Acts" or "Endgame", like the landmark sections
    std::string area = "*";         // an area key, or "*" for every area of the category
    TargetEntry look;               // name, enabled, icon and colours; expectedCount 0 = all, N = nearest N
};

// The rooms one room target marks: every room whose name matches the pattern, or with count > 0
// the count rooms nearest to (px, py). Room is PluginSDK::AreaRoom; a template so the self-test
// can pass a stand-in without the host SDK.
template <class Room>
std::vector<const Room*> SelectRoomTargets(const std::vector<Room>& rooms, const std::string& pattern,
                                           int count, float px, float py) {
    std::vector<const Room*> out;
    for (const auto& room : rooms)
        if (room.Matches(pattern)) out.push_back(&room);
    if (count > 0 && static_cast<size_t>(count) < out.size()) {
        auto distSq = [&](const Room* r) {
            const float dx = r->GridX - px, dy = r->GridY - py;
            return dx * dx + dy * dy;
        };
        std::partial_sort(out.begin(), out.begin() + count, out.end(),
                          [&](const Room* a, const Room* b) { return distSq(a) < distSq(b); });
        out.resize(static_cast<size_t>(count));
    }
    return out;
}

// config/targets/rooms.json
class RoomTargetStore {
public:
    std::vector<RoomTarget> targets;

    static std::filesystem::path PathFor(const std::filesystem::path& pluginDir) {
        return pluginDir / "config" / "targets" / "rooms.json";
    }

    static RoomTarget Parse(const nlohmann::json& o) {
        RoomTarget r;
        r.look = TargetDatabase::ParseTarget(o, "User");
        r.pattern = o.value("Pattern", "");
        r.category = o.value("Category", std::string("Acts")) == "Endgame" ? "Endgame" : "Acts";
        r.area = NormalizeAreaKey(o.value("Area", std::string("*")));
        return r;
    }

    void Load(const std::filesystem::path& pluginDir) {
        targets.clear();
        const auto path = PathFor(pluginDir);
        if (!std::filesystem::exists(path)) return;
        std::ifstream in(path);
        try {
            nlohmann::json j;
            in >> j;
            for (const auto& o : j.at("RoomTargets")) {
                auto r = Parse(o);
                if (!r.pattern.empty()) targets.push_back(std::move(r));
            }
        } catch (const std::exception& ex) {
            RadarLog::Instance().Warn("Skipping room targets " + path.string() + ": " + ex.what());
            targets.clear();
            BackupUnreadableFile(path);
        }
    }

    bool Save(const std::filesystem::path& pluginDir) const {
        nlohmann::json j;
        j["SchemaVersion"] = 1;
        j["RoomTargets"] = nlohmann::json::array();
        for (const auto& r : targets) {
            auto o = TargetDatabase::WriteTarget(r.look);
            o.erase("Path");
            o["Pattern"] = r.pattern;
            o["Category"] = r.category;
            o["Area"] = r.area;
            j["RoomTargets"].push_back(std::move(o));
        }
        const auto path = PathFor(pluginDir);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (WriteFileAtomic(path, j.dump(4))) return true;
        RadarLog::Instance().Warn("Failed to save room targets: " + path.string());
        return false;
    }

    // The enabled targets for an area: its own plus the global ones of its category.
    std::vector<const RoomTarget*> ForArea(std::string_view areaKey, bool endgame) const {
        std::vector<const RoomTarget*> out;
        for (const auto& r : targets)
            if (r.look.enabled && r.category == (endgame ? "Endgame" : "Acts")
                && (r.area == "*" || AreaKeysEqual(r.area, areaKey)))
                out.push_back(&r);
        return out;
    }

    // Moves room targets an earlier build kept in user.json (with RoomPattern; "*" = all areas)
    // here. Returns how many; the caller then saves both files.
    size_t Adopt(TargetDatabase& db) {
        for (const auto& [area, o] : db.legacyRoomTargets) {
            RoomTarget r = Parse(o);
            r.pattern = o.value("RoomPattern", "");
            r.area = area;
            r.category = db.IsEndgameArea(area) ? "Endgame" : "Acts";
            if (!r.pattern.empty()) targets.push_back(std::move(r));
        }
        const size_t n = db.legacyRoomTargets.size();
        db.legacyRoomTargets.clear();
        return n;
    }
};

} // namespace RadarData
