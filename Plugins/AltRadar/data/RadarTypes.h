#pragma once

#include <imgui.h>

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace RadarData {

struct Rgba8 {
    uint8_t r = 255, g = 255, b = 255, a = 255;
    ImU32 ToImU32() const { return IM_COL32(r, g, b, a); }
    ImVec4 ToImVec4() const {
        return ImVec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
    }
    static Rgba8 FromImVec4(const ImVec4& v) {
        auto toByte = [](float x) -> uint8_t {
            const float clamped = x < 0.f ? 0.f : (x > 1.f ? 1.f : x);
            return static_cast<uint8_t>(clamped * 255.f + 0.5f);
        };
        return Rgba8{toByte(v.x), toByte(v.y), toByte(v.z), toByte(v.w)};
    }
};

enum class MarkerShape : uint8_t {
    None = 0,
    Circle,
    Square,
    Triangle,
    Diamond,
    Plus,
    Star,
    Hexagon,
    Pentagon,
    TriangleDown,
    ArrowUp,
    Cross,
    Heart,
    Droplet,
    Gem,
    Fang,
    Claw,
    Shield,
    Flask,
    Eye,
    Coin,
    Sword,
    Skull,
    Person,
    Chat,
    Chest,
    Crown,
    Ring,
    MapPin,
    Flag,
    Stairs,
    Portal,
    Exclamation,
};

inline const char* MarkerShapeName(MarkerShape shape) {
    switch (shape) {
        case MarkerShape::Circle:
            return "Circle";
        case MarkerShape::Square:
            return "Square";
        case MarkerShape::Triangle:
            return "Triangle";
        case MarkerShape::Diamond:
            return "Diamond";
        case MarkerShape::Plus:
            return "Plus";
        case MarkerShape::Star:
            return "Star";
        case MarkerShape::Hexagon:
            return "Hexagon";
        case MarkerShape::Pentagon:
            return "Pentagon";
        case MarkerShape::TriangleDown:
            return "TriangleDown";
        case MarkerShape::ArrowUp:
            return "ArrowUp";
        case MarkerShape::Cross:
            return "Cross";
        case MarkerShape::Heart:
            return "Heart";
        case MarkerShape::Droplet:
            return "Droplet";
        case MarkerShape::Gem:
            return "Gem";
        case MarkerShape::Fang:
            return "Fang";
        case MarkerShape::Claw:
            return "Claw";
        case MarkerShape::Shield:
            return "Shield";
        case MarkerShape::Flask:
            return "Flask";
        case MarkerShape::Eye:
            return "Eye";
        case MarkerShape::Coin:
            return "Coin";
        case MarkerShape::Sword:
            return "Sword";
        case MarkerShape::Skull:
            return "Skull";
        case MarkerShape::Person:
            return "Person";
        case MarkerShape::Chat:
            return "Chat";
        case MarkerShape::Chest:
            return "Chest";
        case MarkerShape::Crown:
            return "Crown";
        case MarkerShape::Ring:
            return "Ring";
        case MarkerShape::MapPin:
            return "MapPin";
        case MarkerShape::Flag:
            return "Flag";
        case MarkerShape::Stairs:
            return "Stairs";
        case MarkerShape::Portal:
            return "Portal";
        case MarkerShape::Exclamation:
            return "Exclamation";
        default:
            return "None";
    }
}

// ASCII lower-case: game paths and rule terms are ASCII, and std::tolower is a locale call per char.
inline char LowerAscii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c; }

inline bool MarkerShapeNameEquals(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (size_t i = 0; i < lhs.size(); ++i)
        if (LowerAscii(lhs[i]) != LowerAscii(rhs[i])) return false;
    return true;
}

inline MarkerShape ParseMarkerShape(std::string_view name) {
    if (MarkerShapeNameEquals(name, "Circle")) return MarkerShape::Circle;
    if (MarkerShapeNameEquals(name, "Square")) return MarkerShape::Square;
    if (MarkerShapeNameEquals(name, "Triangle")) return MarkerShape::Triangle;
    if (MarkerShapeNameEquals(name, "Diamond")) return MarkerShape::Diamond;
    if (MarkerShapeNameEquals(name, "Plus")) return MarkerShape::Plus;
    if (MarkerShapeNameEquals(name, "Star")) return MarkerShape::Star;
    if (MarkerShapeNameEquals(name, "Hexagon")) return MarkerShape::Hexagon;
    if (MarkerShapeNameEquals(name, "Pentagon")) return MarkerShape::Pentagon;
    if (MarkerShapeNameEquals(name, "TriangleDown")) return MarkerShape::TriangleDown;
    if (MarkerShapeNameEquals(name, "ArrowUp")) return MarkerShape::ArrowUp;
    if (MarkerShapeNameEquals(name, "Cross")) return MarkerShape::Cross;
    if (MarkerShapeNameEquals(name, "Heart")) return MarkerShape::Heart;
    if (MarkerShapeNameEquals(name, "Droplet")) return MarkerShape::Droplet;
    if (MarkerShapeNameEquals(name, "Gem")) return MarkerShape::Gem;
    if (MarkerShapeNameEquals(name, "Fang")) return MarkerShape::Fang;
    if (MarkerShapeNameEquals(name, "Claw")) return MarkerShape::Claw;
    if (MarkerShapeNameEquals(name, "Shield")) return MarkerShape::Shield;
    if (MarkerShapeNameEquals(name, "Flask")) return MarkerShape::Flask;
    if (MarkerShapeNameEquals(name, "Eye")) return MarkerShape::Eye;
    if (MarkerShapeNameEquals(name, "Coin")) return MarkerShape::Coin;
    if (MarkerShapeNameEquals(name, "Sword")) return MarkerShape::Sword;
    if (MarkerShapeNameEquals(name, "Skull")) return MarkerShape::Skull;
    if (MarkerShapeNameEquals(name, "Person")) return MarkerShape::Person;
    if (MarkerShapeNameEquals(name, "Chat")) return MarkerShape::Chat;
    if (MarkerShapeNameEquals(name, "Chest")) return MarkerShape::Chest;
    if (MarkerShapeNameEquals(name, "Crown")) return MarkerShape::Crown;
    if (MarkerShapeNameEquals(name, "Ring")) return MarkerShape::Ring;
    if (MarkerShapeNameEquals(name, "MapPin")) return MarkerShape::MapPin;
    if (MarkerShapeNameEquals(name, "Flag")) return MarkerShape::Flag;
    if (MarkerShapeNameEquals(name, "Stairs")) return MarkerShape::Stairs;
    if (MarkerShapeNameEquals(name, "Portal")) return MarkerShape::Portal;
    if (MarkerShapeNameEquals(name, "Exclamation")) return MarkerShape::Exclamation;
    return MarkerShape::None;
}

struct IconDef {
    int         cx = 0;
    int         cy = 0;
    float       scale = 30.f;
    MarkerShape markerShape = MarkerShape::None;
    Rgba8       markerColor{};
    std::string label;
};

struct DisplayRule {
    bool                     enabled = true;
    bool                     hide = false;
    std::string              id;
    std::string              source;
    std::string              name;
    std::vector<std::string> categories;
    std::vector<std::string> subtypes;
    std::vector<std::string> states;
    std::vector<std::string> matchTerms;
    std::vector<std::string> mods;
    std::vector<std::string> models;  // terms matched against the entity's model (.ao) path
    std::string              rarity;
    std::string              reaction;
    std::string              life;
    std::string              chest;
    std::string              poi;
    // Persisted/exposed for future rule filtering work; runtime matching does not use this yet.
    std::string              encounter;
    MarkerShape              markerShape = MarkerShape::Circle;
    bool                     useRuneshapeColor = false;
    Rgba8                    markerColor{255, 217, 38, 255};
    float                    size = 6.f;
    std::string              label;
    bool                     rememberUntilZone = false;
    // Persisted/exposed for future GPS/pathing work; runtime matching does not use this yet.
    bool                     navigable = false;
};

// Called for every rule term of every entity on each refresh, so kept cheap: find the term's first
// character, then compare the rest.
inline bool ContainsCaseInsensitiveRuleText(std::string_view text, std::string_view term) {
    if (term.empty()) return true;
    if (term.size() > text.size()) return false;
    const char first = LowerAscii(term[0]);
    const size_t last = text.size() - term.size();
    for (size_t i = 0; i <= last; ++i) {
        if (LowerAscii(text[i]) != first) continue;
        size_t j = 1;
        while (j < term.size() && LowerAscii(text[i + j]) == LowerAscii(term[j])) ++j;
        if (j == term.size()) return true;
    }
    return false;
}

// How a display rule term matches text (path, mod, model): a term with * or ? matches the
// WHOLE text as a glob, any other term matches as a case-insensitive substring.
inline bool HasGlobWildcard(std::string_view pattern) {
    return pattern.find('*') != std::string_view::npos
           || pattern.find('?') != std::string_view::npos;
}

inline bool MatchGlobCaseInsensitive(std::string_view text, std::string_view pattern) {
    size_t ti = 0;
    size_t pi = 0;
    size_t starPi = std::string_view::npos;
    size_t starTi = 0;
    while (ti < text.size()) {
        if (pi < pattern.size()
            && (pattern[pi] == '?'
                || std::tolower(static_cast<unsigned char>(pattern[pi]))
                       == std::tolower(static_cast<unsigned char>(text[ti])))) {
            ++ti;
            ++pi;
            continue;
        }
        if (pi < pattern.size() && pattern[pi] == '*') {
            starPi = pi++;
            starTi = ti;
            continue;
        }
        if (starPi != std::string_view::npos) {
            pi = starPi + 1;
            ti = ++starTi;
            continue;
        }
        return false;
    }
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

inline bool MatchesRuleTerm(std::string_view text, std::string_view term) {
    if (term.empty()) return true;
    return HasGlobWildcard(term) ? MatchGlobCaseInsensitive(text, term)
                                 : ContainsCaseInsensitiveRuleText(text, term);
}

// Kind of an Azmeri wisp deposit from its model token (same tokens as the stock wisp rules).
inline constexpr const char* kWispKindNames[] = {"Vivid", "Primal", "Wild", "Sacred", "Unknown"};
inline int WispKindIndex(std::string_view model) {
    if (ContainsCaseInsensitiveRuleText(model, "wisp_warden")) return 0;
    if (ContainsCaseInsensitiveRuleText(model, "wisp_primal")) return 1;
    if (ContainsCaseInsensitiveRuleText(model, "wisp_vodoo")) return 2;
    if (ContainsCaseInsensitiveRuleText(model, "wisp_sacred")) return 3;
    return 4;
}

// Marker scale for an Azmeri wisp deposit from its model, wisp_<kind>_<sml|med|big>.ao
// (AzmeriWoodsDustType.WispAnimations, 0.5.5); 1 when the size is not in the name.
inline float WispSizeScale(std::string_view model) {
    if (ContainsCaseInsensitiveRuleText(model, "_sml")) return 0.70f;
    if (ContainsCaseInsensitiveRuleText(model, "_med")) return 0.85f;
    return 1.f;
}

inline bool IsRuneshapeColourEligible(const DisplayRule& rule) {
    for (const auto& term : rule.matchTerms) {
        if (ContainsCaseInsensitiveRuleText(term, "Expedition2/Expedition2Encounter")
            || ContainsCaseInsensitiveRuleText(term, "Expedition2Encounter"))
            return true;
        if (ContainsCaseInsensitiveRuleText(term, "Expedition2")) {
            for (const auto& category : rule.categories) {
                if (MarkerShapeNameEquals(category, "Object")) return true;
            }
        }
    }
    return MarkerShapeNameEquals(rule.name, "Expedition")
           || MarkerShapeNameEquals(rule.label, "Expedition")
           || MarkerShapeNameEquals(rule.id, "stock.expedition");
}

inline MarkerShape DefaultTargetMarkerShape() { return MarkerShape::Sword; }

inline Rgba8 DefaultTargetMarkerColor() { return {255, 140, 0, 255}; }

struct TargetEntry {
    std::string name;
    std::string path;
    bool        enabled = true;
    bool        showIcon = false;
    std::string iconName;
    float       iconSize = 30.f;
    MarkerShape markerShape = DefaultTargetMarkerShape();
    Rgba8       markerColor = DefaultTargetMarkerColor();
    Rgba8       nameColor{253, 224, 71, 255};
    Rgba8       bgColor{0, 0, 0, 255};
    int         expectedCount = 1;
    std::string category;
    bool        hasAnchor = false;
    float       anchorGridX = 0.f;
    float       anchorGridY = 0.f;
    int         anchorTileX = 0;
    int         anchorTileY = 0;
    bool        drawPath = false;  // PathFinder draws the way to it
};

struct PoiResolved {
    std::string name;
    float       gridX = 0.f;
    float       gridY = 0.f;
    float       terrainZ = 0.f;
    bool        fromTgt = true;
    std::vector<std::pair<float, float>> metatileCells;
    float       screenX = 0.f;
    float       screenY = 0.f;
    bool        hasScreen = false;
    bool        showIcon = false;
    float       iconSize = 30.f;
    int         iconCx = 1;
    int         iconCy = 37;
    MarkerShape markerShape = MarkerShape::Circle;
    Rgba8       markerColor;
    Rgba8       nameColor;
    Rgba8       bgColor;
    bool        drawPath = false;
};

struct EntityDrawCmd {
    float gridX = 0.f;
    float gridY = 0.f;
    float terrainZ = 0.f;
    MarkerShape markerShape = MarkerShape::None;
    float       markerSize = 0.f;
    ImU32       markerColor = 0;
    std::string label;
    float       labelScale = 1.f;
};

} // namespace RadarData
