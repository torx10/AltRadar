#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace RadarData {

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

} // namespace RadarData
