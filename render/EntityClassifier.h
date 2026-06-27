#pragma once

#include "data/IconTables.h"
#include "data/RadarConfig.h"
#include "data/RadarTypes.h"
#include "sdk/PluginSDK.h"

#include <algorithm>
#include <optional>

namespace RadarRender {

struct EntityMarkerStyle {
    RadarData::MarkerShape shape = RadarData::MarkerShape::None;
    ImU32                  color = 0;
    float                  size = 0.f;
    std::string            label;
};

struct EntityClassification {
    bool              matched = false;
    bool              hidden = false;
    EntityMarkerStyle style{};
};

inline ImU32 MarkerColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return IM_COL32(r, g, b, a);
}

inline ImU32 MarkerColor(const RadarData::Rgba8& color) { return color.ToImU32(); }

inline const RadarData::IconDef* FindIconDef(
    const std::unordered_map<std::string, RadarData::IconDef>& map, const char* key) {
    const auto it = map.find(key);
    return it != map.end() ? &it->second : nullptr;
}

inline float ResolveMarkerSize(const RadarData::IconDef* def, float defaultAtlasScale,
                               float defaultMarkerSize) {
    if (!def) return defaultMarkerSize;
    if (def->scale <= 0.f) return 0.f;
    if (defaultAtlasScale <= 0.01f) return defaultMarkerSize;
    const float scaled = defaultMarkerSize * (def->scale / defaultAtlasScale);
    return std::clamp(scaled, 1.5f, 22.f);
}

inline RadarData::MarkerShape ResolveMarkerShape(const RadarData::IconDef* def,
                                                 RadarData::MarkerShape fallbackShape) {
    if (def && def->markerShape != RadarData::MarkerShape::None) return def->markerShape;
    return fallbackShape;
}

inline std::optional<EntityMarkerStyle> BuildMarker(RadarData::MarkerShape fallbackShape,
                                                    ImU32 color, float defaultMarkerSize,
                                                    float defaultAtlasScale,
                                                    const RadarData::IconDef* def) {
    const float size = ResolveMarkerSize(def, defaultAtlasScale, defaultMarkerSize);
    const auto shape = ResolveMarkerShape(def, fallbackShape);
    std::string label;
    if (def) color = MarkerColor(def->markerColor);
    if (def) label = def->label;
    if (shape == RadarData::MarkerShape::None || size <= 0.f) return std::nullopt;
    return EntityMarkerStyle{shape, color, size, label};
}

inline const char* EntityCategoryName(const PluginSDK::Entity& e) {
    using ET = PluginSDK::EntityType;
    if (e.EntitySubtype == PluginSDK::EntitySubtype::PlayerOther
        || e.EntitySubtype == PluginSDK::EntitySubtype::PlayerSelf)
        return "Player";
    switch (e.EntityType) {
        case ET::Monster:
            return "Monster";
        case ET::Chest:
            return "Chest";
        case ET::NPC:
            return "Npc";
        case ET::AreaTransition:
            return "Transition";
        case ET::Shrine:
        case ET::OtherImportant:
        case ET::DeliriumBomb:
        case ET::DeliriumSpawner:
        case ET::ExpeditionMarker:
        case ET::ExpeditionRemnant:
            return "Object";
        default:
            return "Other";
    }
}

inline const char* EntitySubtypeName(PluginSDK::EntitySubtype subtype) {
    using ES = PluginSDK::EntitySubtype;
    switch (subtype) {
        case ES::PlayerSelf: return "PlayerSelf";
        case ES::PlayerOther: return "PlayerOther";
        case ES::ChestMagic: return "ChestMagic";
        case ES::ChestRare: return "ChestRare";
        case ES::ExpeditionChest: return "ExpeditionChest";
        case ES::BreachChest: return "BreachChest";
        case ES::Strongbox: return "Strongbox";
        case ES::JewellerStrongbox: return "JewellerStrongbox";
        case ES::ResearcherStrongbox: return "ResearcherStrongbox";
        case ES::LargeStrongbox: return "LargeStrongbox";
        case ES::OmenChest: return "OmenChest";
        case ES::SpecialNPC: return "SpecialNPC";
        case ES::POIMonster: return "POIMonster";
        case ES::PinnacleBoss: return "PinnacleBoss";
        default: return "";
    }
}

inline const char* EntityStateName(PluginSDK::EntityState state) {
    using EST = PluginSDK::EntityState;
    switch (state) {
        case EST::PlayerLeader: return "PlayerLeader";
        case EST::MonsterFriendly: return "MonsterFriendly";
        case EST::PinnacleBossHidden: return "PinnacleBossHidden";
        default: return "";
    }
}

inline bool ContainsCaseInsensitive(std::string_view text, std::string_view term) {
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
                                 : ContainsCaseInsensitive(text, term);
}

inline bool MatchesNamedList(const std::vector<std::string>& values, std::string_view candidate) {
    if (values.empty()) return true;
    if (candidate.empty()) return false;
    for (const auto& value : values) {
        if (RadarData::MarkerShapeNameEquals(value, candidate)) return true;
    }
    return false;
}

inline bool MatchesDisplayRule(const RadarData::DisplayRule& rule, const PluginSDK::Entity& e,
                               std::string_view path) {
    if (!rule.enabled) return false;
    if (!rule.categories.empty()) {
        bool categoryMatch = false;
        const char* category = EntityCategoryName(e);
        for (const auto& wanted : rule.categories) {
            if (RadarData::MarkerShapeNameEquals(wanted, category)) {
                categoryMatch = true;
                break;
            }
        }
        if (!categoryMatch) return false;
    }
    if (!MatchesNamedList(rule.subtypes, EntitySubtypeName(e.EntitySubtype))) return false;
    if (!MatchesNamedList(rule.states, EntityStateName(e.EntityState))) return false;
    if (!rule.matchTerms.empty()) {
        bool any = false;
        for (const auto& term : rule.matchTerms) {
            if (MatchesRuleTerm(path, term)) {
                any = true;
                break;
            }
        }
        if (!any) return false;
    }
    return true;
}

inline EntityClassification ClassifyByRules(const PluginSDK::Entity& e, PluginSDK::Context* ctx,
                                            std::string_view path,
                                            const RadarData::IconTables& icons) {
    for (const auto& rule : icons.displayRules) {
        if (!MatchesDisplayRule(rule, e, path)) continue;
        bool isFriendly = false;
        if (ctx && e.Components.Positioned)
            isFriendly = ctx->Components.ReadPositioned(e.Components.Positioned).IsFriendly;
        else
            isFriendly = e.EntityState == PluginSDK::EntityState::MonsterFriendly;
        if (!rule.rarity.empty()) {
            const char* rarity = e.Rarity >= 3 ? "Unique" : e.Rarity == 2 ? "Rare" : e.Rarity == 1 ? "Magic" : "Normal";
            if (!RadarData::MarkerShapeNameEquals(rule.rarity, rarity)) continue;
        }
        if (!rule.reaction.empty()) {
            const bool wantsFriendly = RadarData::MarkerShapeNameEquals(rule.reaction, "Friendly");
            const bool wantsHostile = RadarData::MarkerShapeNameEquals(rule.reaction, "Hostile");
            if ((wantsFriendly && !isFriendly) || (wantsHostile && isFriendly)) continue;
        }
        if (!rule.life.empty()) {
            const bool alive = e.CurrentHP > 0;
            const bool wantsAlive = RadarData::MarkerShapeNameEquals(rule.life, "Alive");
            const bool wantsDead = RadarData::MarkerShapeNameEquals(rule.life, "Dead");
            if ((wantsAlive && !alive) || (wantsDead && alive)) continue;
        }
        if (!rule.chest.empty()) {
            const bool wantsOpened = RadarData::MarkerShapeNameEquals(rule.chest, "Opened");
            const bool wantsUnopened = RadarData::MarkerShapeNameEquals(rule.chest, "Unopened");
            if ((wantsOpened && !e.IsChestOpened) || (wantsUnopened && e.IsChestOpened)) continue;
        }
        if (!rule.poi.empty()) {
            const bool hasPoi = e.Components.MinimapIcon != 0;
            const bool wantsPoi = RadarData::MarkerShapeNameEquals(rule.poi, "Yes");
            const bool wantsNoPoi = RadarData::MarkerShapeNameEquals(rule.poi, "No");
            if ((wantsPoi && !hasPoi) || (wantsNoPoi && hasPoi)) continue;
        }
        if (!rule.mods.empty()) {
            if (!ctx || e.Components.OMP == 0) continue;
            const auto mods = ctx->Components.EnumerateMonsterMods(e.Components.OMP);
            bool anyMod = false;
            for (const auto& term : rule.mods) {
                for (const auto& mod : mods) {
                    if (MatchesRuleTerm(mod.Id, term) || MatchesRuleTerm(mod.Name, term)
                        || MatchesRuleTerm(mod.Metadata, term)) {
                        anyMod = true;
                        break;
                    }
                }
                if (anyMod) break;
            }
            if (!anyMod) continue;
        }
        EntityClassification out;
        out.matched = true;
        out.hidden = rule.hide || rule.size <= 0.f;
        out.style = EntityMarkerStyle{rule.markerShape, rule.markerColor.ToImU32(),
                                      std::clamp(rule.size, 1.5f, 22.f), rule.label};
        return out;
    }
    return {};
}

inline std::optional<EntityMarkerStyle> BuildBaseMarker(const RadarData::IconTables& icons,
                                                        const char* key,
                                                        RadarData::MarkerShape shape,
                                                        ImU32 color,
                                                        float defaultMarkerSize,
                                                        float defaultAtlasScale) {
    return BuildMarker(shape, color, defaultMarkerSize, defaultAtlasScale,
                       FindIconDef(icons.baseIcons, key));
}

inline std::optional<EntityMarkerStyle> BuildChestMarker(const RadarData::IconTables& icons,
                                                         const char* key,
                                                         RadarData::MarkerShape shape,
                                                         ImU32 color,
                                                         float defaultMarkerSize,
                                                         float defaultAtlasScale) {
    return BuildMarker(shape, color, defaultMarkerSize, defaultAtlasScale,
                       FindIconDef(icons.chestIcons, key));
}

inline std::optional<EntityMarkerStyle> ClassifyLegacySelf(const RadarData::IconTables& icons) {
    return BuildBaseMarker(icons, "Self", RadarData::MarkerShape::Ring,
                           MarkerColor(77, 242, 255), 4.6f, 15.f);
}

inline EntityClassification ClassifyByLegacy(const PluginSDK::Entity& e, std::string_view path,
                                             const RadarData::IconTables& icons) {
    using ET = PluginSDK::EntityType;
    using ES = PluginSDK::EntitySubtype;
    using EST = PluginSDK::EntityState;
    (void)path;

    auto wrap = [](std::optional<EntityMarkerStyle> style) {
        EntityClassification out;
        if (style) {
            out.matched = true;
            out.style = *style;
        }
        return out;
    };

    if (e.EntityState == EST::PlayerLeader) {
        return wrap(BuildBaseMarker(icons, "Leader", RadarData::MarkerShape::Crown,
                                    MarkerColor(77, 242, 255), 5.2f, 25.f));
    }

    switch (e.EntitySubtype) {
        case ES::PlayerSelf:
            return wrap(ClassifyLegacySelf(icons));
        case ES::PlayerOther:
            return wrap(BuildBaseMarker(icons, "Player", RadarData::MarkerShape::Person,
                                        MarkerColor(77, 242, 255), 3.4f, 15.f));
        case ES::SpecialNPC:
            return wrap(BuildBaseMarker(icons, "Special NPC", RadarData::MarkerShape::Chat,
                                        MarkerColor(255, 217, 51, 242), 4.8f, 30.f));
        case ES::POIMonster:
            return wrap(BuildMarker(RadarData::MarkerShape::Skull, MarkerColor(255, 115, 0), 8.0f,
                                    30.f, &icons.poiMonsterDefault));
        case ES::PinnacleBoss:
            return wrap(BuildBaseMarker(icons, "Pinnacle Boss Not Attackable",
                                        RadarData::MarkerShape::Skull,
                                        MarkerColor(255, 115, 0), 9.0f, 30.f));
        case ES::Strongbox:
            return wrap(BuildChestMarker(icons, "Strongbox", RadarData::MarkerShape::Chest,
                                         MarkerColor(255, 179, 0), 6.0f, 30.f));
        case ES::JewellerStrongbox:
            return wrap(BuildChestMarker(icons, "Jeweller's Strongbox",
                                         RadarData::MarkerShape::Chest,
                                         MarkerColor(255, 179, 0), 6.0f, 35.f));
        case ES::ResearcherStrongbox:
            return wrap(BuildChestMarker(icons, "Researcher's Strongbox",
                                         RadarData::MarkerShape::Chest,
                                         MarkerColor(255, 179, 0), 6.0f, 35.f));
        case ES::LargeStrongbox:
            return wrap(BuildChestMarker(icons, "Large Strongbox", RadarData::MarkerShape::Chest,
                                         MarkerColor(255, 179, 0), 6.2f, 35.f));
        case ES::OmenChest:
            return wrap(BuildChestMarker(icons, "Omen Chest", RadarData::MarkerShape::Crown,
                                         MarkerColor(255, 115, 0, 242), 5.5f, 40.f));
        case ES::ChestRare:
            return wrap(BuildChestMarker(icons, "Rare Chests", RadarData::MarkerShape::Chest,
                                         MarkerColor(255, 217, 38, 242), 5.0f, 30.f));
        case ES::ChestMagic:
            return wrap(BuildChestMarker(icons, "Magic Chests", RadarData::MarkerShape::Chest,
                                         MarkerColor(115, 166, 255, 236), 4.4f, 25.f));
        case ES::BreachChest:
            return wrap(BuildMarker(RadarData::MarkerShape::Chest, MarkerColor(242, 89, 242), 5.2f,
                                    20.f, FindIconDef(icons.breachIcons, "Breach Chest")));
        case ES::ExpeditionChest:
            return wrap(BuildMarker(RadarData::MarkerShape::Chest, MarkerColor(38, 230, 217), 5.2f,
                                    60.f, FindIconDef(icons.expeditionIcons, "Generic Expedition Chests")));
        default:
            break;
    }

    if (e.EntityType == ET::Monster) {
        if (e.EntityState == EST::MonsterFriendly) {
            return wrap(BuildBaseMarker(icons, "Friendly", RadarData::MarkerShape::Person,
                                        MarkerColor(102, 255, 153, 235), 3.6f, 15.f));
        }
        if (e.Rarity == 1)
            return wrap(BuildBaseMarker(icons, "Magic Monster", RadarData::MarkerShape::Fang,
                                        MarkerColor(115, 166, 255, 247), 5.5f, 30.f));
        if (e.Rarity == 2)
            return wrap(BuildBaseMarker(icons, "Rare Monster", RadarData::MarkerShape::Claw,
                                        MarkerColor(255, 217, 38), 7.5f, 30.f));
        if (e.Rarity >= 3)
            return wrap(BuildBaseMarker(icons, "Unique Monster", RadarData::MarkerShape::Skull,
                                        MarkerColor(255, 115, 0), 8.0f, 30.f));
        return wrap(BuildBaseMarker(icons, "Normal Monster", RadarData::MarkerShape::Circle,
                                    MarkerColor(255, 51, 51, 242), 2.6f, 30.f));
    }
    if (e.EntityType == ET::NPC) {
        return wrap(BuildBaseMarker(icons, "NPC", RadarData::MarkerShape::Chat,
                                    MarkerColor(255, 217, 51, 242), 4.2f, 15.f));
    }
    if (e.EntityType == ET::Chest) {
        const ImU32 chestColor = e.IsChestOpened ? MarkerColor(128, 148, 148, 176)
                                                 : MarkerColor(140, 191, 255, 224);
        return wrap(BuildChestMarker(icons, "All Other Chest", RadarData::MarkerShape::Chest,
                                     chestColor, 4.4f, 15.f));
    }
    return {};
}

inline std::optional<EntityMarkerStyle> ClassifySelfRule(const RadarData::IconTables& icons) {
    for (const auto& rule : icons.displayRules) {
        if (!rule.enabled) continue;
        if (rule.id == "stock.player_self"
            || std::find(rule.subtypes.begin(), rule.subtypes.end(), "PlayerSelf") != rule.subtypes.end()) {
            if (rule.markerShape == RadarData::MarkerShape::None || rule.size <= 0.f) return std::nullopt;
            return EntityMarkerStyle{rule.markerShape, rule.markerColor.ToImU32(),
                                     std::clamp(rule.size, 1.5f, 22.f), rule.label};
        }
    }
    return std::nullopt;
}

inline EntityClassification ClassifyEntity(PluginSDK::Context* ctx, const PluginSDK::Entity& e,
                                           std::string_view path,
                                           const RadarData::IconTables& icons,
                                           const RadarData::RadarConfig& cfg) {
    return cfg.UseLegacyClassifier ? ClassifyByLegacy(e, path, icons)
                                   : ClassifyByRules(e, ctx, path, icons);
}

inline std::optional<EntityMarkerStyle> ClassifySelf(const RadarData::IconTables& icons,
                                                     const RadarData::RadarConfig& cfg) {
    return cfg.UseLegacyClassifier ? ClassifyLegacySelf(icons) : ClassifySelfRule(icons);
}

} // namespace RadarRender
