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

inline bool MarkerShapeNameEquals(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (size_t i = 0; i < lhs.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(lhs[i]);
        const unsigned char b = static_cast<unsigned char>(rhs[i]);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
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

inline bool ContainsCaseInsensitiveRuleText(std::string_view text, std::string_view term) {
    if (term.empty()) return true;
    if (term.size() > text.size()) return false;
    for (size_t i = 0; i + term.size() <= text.size(); ++i) {
        bool ok = true;
        for (size_t j = 0; j < term.size(); ++j) {
            const unsigned char a = static_cast<unsigned char>(text[i + j]);
            const unsigned char b = static_cast<unsigned char>(term[j]);
            if (std::tolower(a) != std::tolower(b)) {
                ok = false;
                break;
            }
        }
        if (ok) return true;
    }
    return false;
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

struct TargetEntry {
    std::string name;
    std::string path;
    bool        enabled = true;
    bool        showIcon = false;
    std::string iconName;
    float       iconSize = 30.f;
    Rgba8       nameColor{253, 224, 71, 255};
    Rgba8       bgColor{0, 0, 0, 255};
    int         expectedCount = 1;
    std::string category;
    bool        hasAnchor = false;
    float       anchorGridX = 0.f;
    float       anchorGridY = 0.f;
    int         anchorTileX = 0;
    int         anchorTileY = 0;
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
};

struct EntityDrawCmd {
    float gridX = 0.f;
    float gridY = 0.f;
    float terrainZ = 0.f;
    MarkerShape markerShape = MarkerShape::None;
    float       markerSize = 0.f;
    ImU32       markerColor = 0;
    std::string label;
};

} // namespace RadarData
