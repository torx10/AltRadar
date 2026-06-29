#pragma once

#include "data/RadarConfig.h"
#include "data/IconTables.h"
#include "data/TargetDatabase.h"
#include "render/EntityMarkers.h"
#include "render/IconAtlas.h"
#include "render/RadarOverlay.h"

#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <optional>
#include <unordered_set>

namespace RadarUi {

struct UiState {
    bool        shapePickerOpen = false;
    std::string shapePickerTarget;
    std::unordered_map<std::string, RadarData::IconDef>* shapePickerMap = nullptr;
    char        rulePickerSearch[128]{};
    int         rulePickerCategory = 0;
    bool        rulePickerOpen = false;

    bool        pickerPoiMode = false;
    bool        pickerEntityMode = false;
    bool        requestResetSettings = false;
    bool        requestResetCustomLandmarks = false;

    bool        editModalOpen = false;
    RadarData::TargetEntry editTarget;
    std::string editAreaKey;
    bool        editIsNew = false;
    size_t      editStorageIndex = SIZE_MAX;
};

struct RulePickerRow {
    enum class Kind {
        Entity,
        Tile,
        Mod,
    };

    Kind        kind = Kind::Entity;
    std::string name;
    std::string category;
    std::string detail;
    std::string seedValue;
};

inline constexpr RadarData::MarkerShape kMarkerShapes[] = {
    RadarData::MarkerShape::Circle,      RadarData::MarkerShape::Square,
    RadarData::MarkerShape::Triangle,    RadarData::MarkerShape::Diamond,
    RadarData::MarkerShape::Plus,        RadarData::MarkerShape::Star,
    RadarData::MarkerShape::Hexagon,     RadarData::MarkerShape::Pentagon,
    RadarData::MarkerShape::TriangleDown,RadarData::MarkerShape::ArrowUp,
    RadarData::MarkerShape::Cross,       RadarData::MarkerShape::Heart,
    RadarData::MarkerShape::Droplet,     RadarData::MarkerShape::Gem,
    RadarData::MarkerShape::Ring,        RadarData::MarkerShape::Shield,
    RadarData::MarkerShape::Exclamation, RadarData::MarkerShape::Skull,
    RadarData::MarkerShape::Crown,       RadarData::MarkerShape::Person,
    RadarData::MarkerShape::Chat,        RadarData::MarkerShape::Chest,
    RadarData::MarkerShape::Stairs,      RadarData::MarkerShape::MapPin,
    RadarData::MarkerShape::Portal,      RadarData::MarkerShape::Flask,
    RadarData::MarkerShape::Flag,        RadarData::MarkerShape::Eye,
    RadarData::MarkerShape::Coin,        RadarData::MarkerShape::Sword,
    RadarData::MarkerShape::Fang,        RadarData::MarkerShape::Claw,
};

inline void DrawShapePicker(UiState& ui) {
    if (!ui.shapePickerOpen || !ui.shapePickerMap) return;
    auto it = ui.shapePickerMap->find(ui.shapePickerTarget);
    if (it == ui.shapePickerMap->end()) {
        ui.shapePickerOpen = false;
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(560, 360), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Marker Picker", &ui.shapePickerOpen)) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted(ui.shapePickerTarget.c_str());
    ImGui::Separator();

    const int cols = 6;
    constexpr int kShapeCount = static_cast<int>(sizeof(kMarkerShapes) / sizeof(kMarkerShapes[0]));
    for (int i = 0; i < kShapeCount; ++i) {
        const auto shape = kMarkerShapes[i];
        ImGui::PushID(i);
        if (i % cols != 0) ImGui::SameLine();

        const bool selected = it->second.markerShape == shape;
        if (ImGui::InvisibleButton("##shapeCell", ImVec2(84.f, 58.f))) {
            it->second.markerShape = shape;
            ui.shapePickerOpen = false;
        }
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(min, max, IM_COL32(22, 26, 34, 255), 4.f);
        dl->AddRect(min, max,
                    selected ? IM_COL32(90, 155, 235, 255)
                             : (ImGui::IsItemHovered() ? IM_COL32(72, 120, 196, 255)
                                                       : IM_COL32(64, 70, 82, 255)),
                    4.f, 0, selected ? 2.f : 1.f);
        const ImVec2 center((min.x + max.x) * 0.5f, min.y + 18.f);
        RadarRender::DrawEntityMarker(dl, shape, center.x, center.y, 7.f,
                                      IM_COL32(115, 220, 255, 255));
        const char* label = RadarData::MarkerShapeName(shape);
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(center.x - textSize.x * 0.5f, min.y + 36.f),
                    IM_COL32(232, 236, 242, 255), label);
        ImGui::PopID();
    }

    ImGui::End();
}

inline void DrawGeneralTab(RadarData::RadarConfig& cfg, UiState& ui) {
    auto drawTerrainRenderStyleCombo = [&](const char* label) {
        const char* preview = RadarData::TerrainRenderStyleName(cfg.TerrainStyle);
        ImGui::SetNextItemWidth(240.f);
        if (!ImGui::BeginCombo(label, preview, ImGuiComboFlags_HeightLargest)) return;

        const auto drawOption = [&](RadarData::TerrainRenderStyle style) {
            const bool selected = cfg.TerrainStyle == style;
            if (ImGui::Selectable(RadarData::TerrainRenderStyleName(style), selected))
                cfg.TerrainStyle = style;
            if (selected) ImGui::SetItemDefaultFocus();
        };

        drawOption(RadarData::TerrainRenderStyle::Texture);
        drawOption(RadarData::TerrainRenderStyle::DotMatrix);
        drawOption(RadarData::TerrainRenderStyle::TextureAndDotMatrix);
        ImGui::EndCombo();
    };
    bool en = cfg.OverlayEnabled;
    if (ImGui::Checkbox("Enable Radar Overlay", &en)) cfg.OverlayEnabled = en;
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Terrain Overlay", ImGuiTreeNodeFlags_DefaultOpen)) {
        const bool usesTexture = cfg.TerrainStyle == RadarData::TerrainRenderStyle::Texture
                                 || cfg.TerrainStyle == RadarData::TerrainRenderStyle::TextureAndDotMatrix;
        const bool usesDot = cfg.TerrainStyle == RadarData::TerrainRenderStyle::DotMatrix
                             || cfg.TerrainStyle == RadarData::TerrainRenderStyle::TextureAndDotMatrix;
        ImGui::Indent(12.f);
        ImGui::Checkbox("Enable Terrain Overlay", &cfg.DrawWalkableMap);
        ImGui::BeginDisabled(!cfg.DrawWalkableMap);
        drawTerrainRenderStyleCombo("Terrain Render Style");
        if (usesTexture) {
            ImGui::SetNextItemWidth(240.f);
            ImGui::ColorEdit4("Texture Interior Fill", &cfg.TextureInteriorColor.x,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::SetNextItemWidth(240.f);
            ImGui::ColorEdit4("Texture Wall Edge", &cfg.TextureWallEdgeColor.x,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Edge Thickness");
            ImGui::SameLine(160.f);
            ImGui::SetNextItemWidth(140.f);
            ImGui::SliderInt("##EdgeThickness", &cfg.WalkableMapBorderThickness, 0, 8, "%d");
        }
        if (usesDot) {
            ImGui::SetNextItemWidth(240.f);
            ImGui::ColorEdit4("Dot Matrix Fill", &cfg.DotMatrixFillColor.x,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Dot Cell Step");
            ImGui::SameLine(160.f);
            ImGui::SetNextItemWidth(140.f);
            ImGui::SliderInt("##DotCellStep", &cfg.DotCellStep, 1, 16, "%d");
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Dot Size");
            ImGui::SameLine(160.f);
            ImGui::SetNextItemWidth(140.f);
            ImGui::SliderFloat("##DotSize", &cfg.DotSize, 0.5f, 6.0f, "%.1f");
        }
        ImGui::EndDisabled();
        ImGui::Unindent(12.f);
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Minimap Radar", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(12.f);
        ImGui::Checkbox("Enemies", &cfg.DrawMiniMapEntities);
        ImGui::Unindent(12.f);
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Display Conditions", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(12.f);
        ImGui::Checkbox("Hide in Towns/Hideouts", &cfg.DrawWhenNotInHideoutOrTown);
        ImGui::Checkbox("Hide on Pause", &cfg.DrawWhenNotPaused);
        ImGui::Checkbox("Hide when alt-tabbed", &cfg.HideWhenNotForeground);
        ImGui::Unindent(12.f);
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Performance / Entity Limits", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(12.f);
        ImGui::Checkbox("Hide Distant Entities (network bubble)", &cfg.HideOutsideNetworkBubble);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Max entities drawn");
        ImGui::SameLine(160.f);
        ImGui::SetNextItemWidth(160.f);
        ImGui::SliderInt("##MaxEntitiesDrawn", &cfg.MaxEntitiesDrawn, 64, 2048, "%d");
        ImGui::Unindent(12.f);
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Reset / Maintenance", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(12.f);
        if (ImGui::Button("Reset General Settings", ImVec2(180.f, 0.f))) {
            ui.requestResetSettings = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Restore Radar settings to their built-in defaults.\nDoes not change display rules or custom landmarks.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Custom Landmarks", ImVec2(190.f, 0.f))) {
            ui.requestResetCustomLandmarks = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Delete custom map-picked landmarks saved in user.json.\nBundled curated landmarks and display rules stay unchanged.");
        }
        ImGui::Unindent(12.f);
    }
}

inline std::string BaseNameForPath(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

inline const char* DisplayRuleCategoryForEntity(const PluginSDK::Entity& e) {
    if (e.EntitySubtype == PluginSDK::EntitySubtype::PlayerOther
        || e.EntitySubtype == PluginSDK::EntitySubtype::PlayerSelf)
        return "Player";
    switch (e.EntityType) {
        case PluginSDK::EntityType::Monster:
            return "Monster";
        case PluginSDK::EntityType::Chest:
            return "Chest";
        case PluginSDK::EntityType::NPC:
            return "Npc";
        case PluginSDK::EntityType::AreaTransition:
            return "Transition";
        case PluginSDK::EntityType::Shrine:
        case PluginSDK::EntityType::OtherImportant:
        case PluginSDK::EntityType::DeliriumBomb:
        case PluginSDK::EntityType::DeliriumSpawner:
        case PluginSDK::EntityType::ExpeditionMarker:
        case PluginSDK::EntityType::ExpeditionRemnant:
            return "Object";
        default:
            return "Other";
    }
}

inline RadarData::DisplayRule SeedRuleFromEntity(const PluginSDK::Entity& e) {
    const std::string path(e.Path.begin(), e.Path.end());
    const std::string base = BaseNameForPath(path);
    RadarData::DisplayRule rule;
    rule.source = "User";
    rule.name = base.empty() ? "New rule" : base;
    rule.categories = {DisplayRuleCategoryForEntity(e)};
    if (!base.empty()) rule.matchTerms = {base};
    const bool isWaypoint = path.find("Waypoint") != std::string::npos;
    const bool isCheckpoint = path.find("Checkpoint") != std::string::npos;
    if (rule.categories[0] == "Player") {
        rule.markerShape = RadarData::MarkerShape::Person;
        rule.markerColor = {77, 242, 255, 255};
        rule.size = 4.5f;
        rule.reaction = "Friendly";
    } else if (rule.categories[0] == "Npc") {
        rule.markerShape = RadarData::MarkerShape::Chat;
        rule.markerColor = {255, 217, 51, 242};
        rule.size = 5.0f;
        rule.reaction = "Friendly";
    } else if (rule.categories[0] == "Chest") {
        rule.markerShape = RadarData::MarkerShape::Chest;
        rule.markerColor = {255, 179, 0, 255};
        rule.size = 6.0f;
        rule.chest = "Unopened";
    } else if (rule.categories[0] == "Transition") {
        rule.markerShape = RadarData::MarkerShape::Stairs;
        rule.markerColor = {102, 255, 153, 255};
        rule.size = 5.5f;
    } else if (rule.categories[0] == "Object" && isWaypoint) {
        rule.markerShape = RadarData::MarkerShape::MapPin;
        rule.markerColor = {64, 235, 255, 255};
        rule.size = 5.5f;
        rule.poi = "Yes";
    } else if (rule.categories[0] == "Object" && isCheckpoint) {
        rule.markerShape = RadarData::MarkerShape::Flag;
        rule.markerColor = {255, 217, 51, 255};
        rule.size = 5.5f;
        rule.poi = "Yes";
    } else if (rule.categories[0] == "Monster") {
        rule.markerShape = RadarData::MarkerShape::Circle;
        rule.markerColor = {255, 217, 38, 255};
        rule.size = 6.0f;
        if (e.Rarity >= 3) rule.rarity = "Unique";
        else if (e.Rarity == 2) rule.rarity = "Rare";
        else if (e.Rarity == 1) rule.rarity = "Magic";
        else rule.rarity = "Normal";
    }
    return rule;
}

inline RadarData::DisplayRule SeedRuleFromTilePath(std::string_view path) {
    RadarData::DisplayRule rule;
    rule.source = "User";
    const std::string pathStr(path);
    const std::string base = BaseNameForPath(pathStr);
    rule.name = base.empty() ? "Tile rule" : base;
    rule.categories = {"Tile"};
    rule.matchTerms = {pathStr};
    rule.markerShape = RadarData::MarkerShape::Diamond;
    rule.markerColor = {115, 191, 255, 224};
    rule.size = 5.0f;
    return rule;
}

inline RadarData::DisplayRule SeedRuleFromMonsterMod(std::string_view modId) {
    RadarData::DisplayRule rule;
    rule.source = "User";
    rule.name = std::string(modId);
    rule.categories = {"Monster"};
    rule.mods = {std::string(modId)};
    rule.markerShape = RadarData::MarkerShape::Exclamation;
    rule.markerColor = {180, 80, 255, 255};
    rule.size = 6.0f;
    return rule;
}

inline void DrawDisplayRuleCategoryChips(RadarData::DisplayRule& rule, const char* idPrefix) {
    static constexpr const char* kCategories[] = {
        "Monster", "Chest", "Npc", "Object", "Other", "Transition", "Player", "Tile"};
    for (int i = 0; i < static_cast<int>(sizeof(kCategories) / sizeof(kCategories[0])); ++i) {
        if (i != 0) ImGui::SameLine();
        const char* cat = kCategories[i];
        bool checked = std::find(rule.categories.begin(), rule.categories.end(), cat) != rule.categories.end();
        ImGui::PushID((std::string(idPrefix) + cat).c_str());
        ImGui::PushStyleColor(ImGuiCol_Button,
                              checked ? ImVec4(0.38f, 0.29f, 0.08f, 1.f)
                                      : ImVec4(0.10f, 0.10f, 0.10f, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              checked ? ImVec4(0.46f, 0.36f, 0.10f, 1.f)
                                      : ImVec4(0.18f, 0.18f, 0.18f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              checked ? ImVec4(0.32f, 0.24f, 0.06f, 1.f)
                                      : ImVec4(0.12f, 0.12f, 0.12f, 1.f));
        if (ImGui::SmallButton(cat)) {
            if (!checked) {
                if (std::find(rule.categories.begin(), rule.categories.end(), cat)
                    == rule.categories.end())
                    rule.categories.push_back(cat);
            } else {
                rule.categories.erase(
                    std::remove(rule.categories.begin(), rule.categories.end(), cat),
                    rule.categories.end());
            }
        }
        ImGui::PopStyleColor(3);
        ImGui::PopID();
    }
}

inline std::string JoinCsv(const std::vector<std::string>& values) {
    std::string out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) out += ", ";
        out += values[i];
    }
    return out;
}

inline bool ContainsCaseInsensitiveUi(std::string_view text, std::string_view term) {
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

inline void SplitCsv(const char* text, std::vector<std::string>& out) {
    out.clear();
    std::string cur;
    while (*text) {
        if (*text == ',') {
            size_t start = cur.find_first_not_of(" \t");
            size_t end = cur.find_last_not_of(" \t");
            if (start != std::string::npos) out.push_back(cur.substr(start, end - start + 1));
            cur.clear();
        } else {
            cur.push_back(*text);
        }
        ++text;
    }
    size_t start = cur.find_first_not_of(" \t");
    size_t end = cur.find_last_not_of(" \t");
    if (start != std::string::npos) out.push_back(cur.substr(start, end - start + 1));
}

inline void DrawMarkerShapeCombo(const char* label, RadarData::MarkerShape& shape) {
    const std::string preview = std::string("   ") + RadarData::MarkerShapeName(shape);
    if (ImGui::BeginCombo(label, preview.c_str())) {
        for (const auto candidate : kMarkerShapes) {
            const bool selected = candidate == shape;
            const std::string rowId = std::string("##markeropt") + RadarData::MarkerShapeName(candidate);
            if (ImGui::Selectable(rowId.c_str(), selected, 0, ImVec2(0.f, 20.f)))
                shape = candidate;

            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const float cy = (min.y + max.y) * 0.5f;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (dl) {
                RadarRender::DrawEntityMarker(dl, candidate, min.x + 10.f, cy, 5.f,
                                              IM_COL32(232, 220, 184, 255));
                dl->AddText(ImVec2(min.x + 20.f, min.y + 2.f),
                            ImGui::GetColorU32(selected ? ImGuiCol_Text : ImGuiCol_Text),
                            RadarData::MarkerShapeName(candidate));
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (dl) {
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float cy = (min.y + max.y) * 0.5f;
        RadarRender::DrawEntityMarker(dl, shape, min.x + 10.f, cy, 5.f, IM_COL32(232, 220, 184, 255));
    }
}

inline void DrawRuleSelect(const char* label, std::string& value,
                           std::initializer_list<const char*> options, float width = 108.f) {
    ImGui::SetNextItemWidth(width);
    const char* preview = value.empty() ? "any" : value.c_str();
    if (ImGui::BeginCombo(label, preview)) {
        const bool anySelected = value.empty();
        if (ImGui::Selectable("any", anySelected)) value.clear();
        if (anySelected) ImGui::SetItemDefaultFocus();
        for (const char* option : options) {
            const bool selected = value == option;
            if (ImGui::Selectable(option, selected)) value = option;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

inline std::string DisplayRuleSummary(const RadarData::DisplayRule& rule) {
    std::string summary;
    summary += rule.categories.empty() ? "any type" : JoinCsv(rule.categories);
    if (!rule.subtypes.empty()) {
        summary += " · ";
        summary += JoinCsv(rule.subtypes);
    }
    if (!rule.states.empty()) {
        summary += " · ";
        summary += JoinCsv(rule.states);
    }
    if (!rule.matchTerms.empty()) {
        summary += " · \"";
        summary += JoinCsv(rule.matchTerms);
        summary += "\"";
    }
    if (!rule.rarity.empty()) {
        summary += " · ";
        summary += rule.rarity;
    }
    if (!rule.reaction.empty()) {
        summary += " · ";
        summary += rule.reaction;
    }
    if (rule.hide) summary += " · hidden";
    return summary;
}

enum class RuleSection {
    StateHides,
    User,
    Seeded,
};

inline RuleSection GetRuleSection(const RadarData::DisplayRule& rule) {
    if (RadarData::IconTables::IsStateHideRule(rule)) return RuleSection::StateHides;
    return RadarData::MarkerShapeNameEquals(rule.source, "Seeded") ? RuleSection::Seeded
                                                                   : RuleSection::User;
}

inline size_t UserRuleInsertIndex(const std::vector<RadarData::DisplayRule>& rules) {
    size_t index = 0;
    while (index < rules.size() && GetRuleSection(rules[index]) == RuleSection::StateHides) ++index;
    return index;
}

inline std::optional<size_t> FindPrevRuleInSection(const std::vector<RadarData::DisplayRule>& rules,
                                                   size_t index, RuleSection section) {
    while (index > 0) {
        --index;
        if (GetRuleSection(rules[index]) == section) return index;
    }
    return std::nullopt;
}

inline std::optional<size_t> FindNextRuleInSection(const std::vector<RadarData::DisplayRule>& rules,
                                                   size_t index, RuleSection section) {
    for (size_t i = index + 1; i < rules.size(); ++i) {
        if (GetRuleSection(rules[i]) == section) return i;
    }
    return std::nullopt;
}

inline void PushRuleCardStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 4.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.f, 4.f));
}

inline void PopRuleCardStyle() {
    ImGui::PopStyleVar(3);
}

inline void CenterCursorForCell(float width, float height);

inline void DrawRuleMarkerPreview(RadarData::MarkerShape shape, const RadarData::Rgba8& color,
                                  float radius = 6.f) {
    CenterCursorForCell(18.f, radius * 2.f + 2.f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float width = 18.f;
    const float x = p.x + width * 0.5f;
    const float y = p.y + radius + 2.f;
    RadarRender::DrawEntityMarker(dl, shape, x, y, radius,
                                  color.ToImU32());
    ImGui::Dummy(ImVec2(width, radius * 2.f + 2.f));
}

inline void DrawRuleOpacitySlider(const char* label, RadarData::Rgba8& color, float width = 108.f) {
    int opacity = static_cast<int>(color.a * 100 / 255);
    ImGui::SetNextItemWidth(width);
    if (ImGui::SliderInt(label, &opacity, 0, 100, "%d%%"))
        color.a = static_cast<uint8_t>((opacity * 255 + 50) / 100);
}

inline void DrawInlineRuleLabel(const char* label) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(0.f, 4.f);
}

enum class BuiltinRuleGroup {
    Base,
    Chest,
    Breach,
    Delirium,
    Expedition,
};

struct BuiltinRuleRef {
    const char*      title;
    const char*      key;
    const char*      summary;
    BuiltinRuleGroup group;
};

inline RadarData::DisplayRule BuildBuiltinRuleTemplate(const BuiltinRuleRef& ref) {
    RadarData::DisplayRule rule;
    rule.name = ref.title;
    if (std::strcmp(ref.key, "Player") == 0 || std::strcmp(ref.key, "Leader") == 0
        || std::strcmp(ref.key, "Self") == 0) {
        rule.categories = {"Player"};
    } else if (std::strcmp(ref.key, "NPC") == 0 || std::strcmp(ref.key, "Special NPC") == 0) {
        rule.categories = {"Npc"};
    } else if (std::strcmp(ref.key, "Friendly") == 0) {
        rule.categories = {"Monster"};
        rule.reaction = "Friendly";
    } else if (std::strcmp(ref.key, "Normal Monster") == 0) {
        rule.categories = {"Monster"};
        rule.rarity = "Normal";
        rule.reaction = "Hostile";
    } else if (std::strcmp(ref.key, "Magic Monster") == 0) {
        rule.categories = {"Monster"};
        rule.rarity = "Magic";
        rule.reaction = "Hostile";
    } else if (std::strcmp(ref.key, "Rare Monster") == 0) {
        rule.categories = {"Monster"};
        rule.rarity = "Rare";
        rule.reaction = "Hostile";
    } else if (std::strcmp(ref.key, "Unique Monster") == 0
               || std::strcmp(ref.key, "Pinnacle Boss Not Attackable") == 0) {
        rule.categories = {"Monster"};
        rule.rarity = "Unique";
        rule.reaction = "Hostile";
    } else if (std::strcmp(ref.title, "Waypoint") == 0 || std::strcmp(ref.title, "Checkpoint") == 0) {
        rule.categories = {"Object", "Other"};
        rule.poi = "Yes";
        rule.matchTerms = {ref.title};
    } else if (std::strcmp(ref.key, "Strongbox") == 0
               || std::strcmp(ref.key, "Jeweller's Strongbox") == 0
               || std::strcmp(ref.key, "Researcher's Strongbox") == 0
               || std::strcmp(ref.key, "Large Strongbox") == 0
               || std::strcmp(ref.key, "Omen Chest") == 0
               || std::strcmp(ref.key, "Rare Chests") == 0
               || std::strcmp(ref.key, "Magic Chests") == 0
               || std::strcmp(ref.key, "All Other Chest") == 0
               || std::strcmp(ref.key, "Breach Chest") == 0) {
        rule.categories = {"Chest"};
        rule.matchTerms = {ref.key};
        if (std::strcmp(ref.key, "Rare Chests") == 0) rule.rarity = "Rare";
        if (std::strcmp(ref.key, "Magic Chests") == 0) rule.rarity = "Magic";
        if (std::strcmp(ref.key, "Omen Chest") == 0) rule.rarity = "Unique";
    } else {
        rule.categories = {"Object", "Other"};
        rule.matchTerms = {ref.key};
    }
    return rule;
}

inline void DrawDisplayRuleMatcherFields(RadarData::DisplayRule& rule, bool editable,
                                         bool labelEditable, const char* idPrefix) {
    char nameBuf[256]{};
    strncpy_s(nameBuf, rule.name.c_str(), sizeof(nameBuf) - 1);
    ImGui::BeginDisabled(!editable);
    ImGui::SetNextItemWidth(250.f);
    if (ImGui::InputTextWithHint((std::string(idPrefix) + "##name").c_str(), "rule name",
                                 nameBuf, sizeof(nameBuf)))
        rule.name = nameBuf;
    ImGui::EndDisabled();

    ImGui::SameLine();
    char labelBuf[256]{};
    strncpy_s(labelBuf, rule.label.c_str(), sizeof(labelBuf) - 1);
    ImGui::BeginDisabled(!labelEditable);
    ImGui::SetNextItemWidth(250.f);
    if (ImGui::InputTextWithHint((std::string(idPrefix) + "##label").c_str(),
                                 "label (optional)", labelBuf, sizeof(labelBuf)))
        rule.label = labelBuf;
    ImGui::EndDisabled();
    ImGui::Spacing();

    std::string matchCsv = JoinCsv(rule.matchTerms);
    char matchBuf[512]{};
    strncpy_s(matchBuf, matchCsv.c_str(), sizeof(matchBuf) - 1);
    ImGui::BeginDisabled(!editable);
    ImGui::SetNextItemWidth(380.f);
    if (ImGui::InputTextWithHint((std::string(idPrefix) + "##match").c_str(),
                                 "metadata terms, comma-separated (blank = any)", matchBuf,
                                 sizeof(matchBuf)))
        SplitCsv(matchBuf, rule.matchTerms);
    ImGui::EndDisabled();

    ImGui::SameLine();
    std::string modsCsv = JoinCsv(rule.mods);
    char modsBuf[512]{};
    strncpy_s(modsBuf, modsCsv.c_str(), sizeof(modsBuf) - 1);
    ImGui::BeginDisabled(!editable);
    ImGui::SetNextItemWidth(380.f);
    if (ImGui::InputTextWithHint((std::string(idPrefix) + "##mods").c_str(),
                                 "monster mods: aura/buff/mod ids, comma-separated (blank = any)",
                                 modsBuf, sizeof(modsBuf)))
        SplitCsv(modsBuf, rule.mods);
    ImGui::EndDisabled();
    ImGui::Spacing();

    if (!rule.subtypes.empty()) {
        const std::string subtypeCsv = JoinCsv(rule.subtypes);
        char subtypeBuf[256]{};
        strncpy_s(subtypeBuf, subtypeCsv.c_str(), sizeof(subtypeBuf) - 1);
        ImGui::BeginDisabled(true);
        ImGui::SetNextItemWidth(380.f);
        ImGui::InputTextWithHint((std::string(idPrefix) + "##subtypes").c_str(),
                                 "entity subtypes", subtypeBuf, sizeof(subtypeBuf));
        ImGui::EndDisabled();
        ImGui::Spacing();
    }
    if (!rule.states.empty()) {
        const std::string stateCsv = JoinCsv(rule.states);
        char stateBuf[256]{};
        strncpy_s(stateBuf, stateCsv.c_str(), sizeof(stateBuf) - 1);
        ImGui::BeginDisabled(true);
        ImGui::SetNextItemWidth(380.f);
        ImGui::InputTextWithHint((std::string(idPrefix) + "##states").c_str(),
                                 "entity states", stateBuf, sizeof(stateBuf));
        ImGui::EndDisabled();
        ImGui::Spacing();
    }

    ImGui::BeginDisabled(!editable);
    DrawDisplayRuleCategoryChips(rule, idPrefix);
    if (!editable && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("System-rule type filters are fixed by Radar's built-in classifier.");
    }
    ImGui::EndDisabled();
    ImGui::Spacing();

    ImGui::BeginDisabled(!editable);
    DrawInlineRuleLabel("Rarity");
    DrawRuleSelect((std::string(idPrefix) + "##Rarity").c_str(), rule.rarity,
                   {"Normal", "Magic", "Rare", "Unique"}, 92.f);
    ImGui::SameLine();
    DrawInlineRuleLabel("Reaction");
    DrawRuleSelect((std::string(idPrefix) + "##Reaction").c_str(), rule.reaction,
                   {"Hostile", "Friendly"}, 92.f);
    ImGui::SameLine();
    DrawInlineRuleLabel("Life");
    DrawRuleSelect((std::string(idPrefix) + "##Life").c_str(), rule.life, {"Alive", "Dead"}, 84.f);
    ImGui::SameLine(0.f, 12.f);
    DrawInlineRuleLabel("Chest");
    DrawRuleSelect((std::string(idPrefix) + "##Chest").c_str(), rule.chest,
                   {"Opened", "Unopened"}, 92.f);
    ImGui::SameLine();
    DrawInlineRuleLabel("POI");
    DrawRuleSelect((std::string(idPrefix) + "##POI").c_str(), rule.poi, {"Yes", "No"}, 74.f);
    if (!editable && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("System-rule match conditions are fixed by Radar's built-in "
                          "classifier. The action style below is editable.");
    }
    ImGui::EndDisabled();
    ImGui::Spacing();
}

inline bool BeginRuleHeaderTable(const char* id) {
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings
                            | ImGuiTableFlags_BordersInnerV;
    if (!ImGui::BeginTable(id, 6, flags)) return false;
    ImGui::TableSetupColumn("Expand", ImGuiTableColumnFlags_WidthFixed, 14.f);
    ImGui::TableSetupColumn("Toggle", ImGuiTableColumnFlags_WidthFixed, 28.f);
    ImGui::TableSetupColumn("Marker", ImGuiTableColumnFlags_WidthFixed, 26.f);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.34f);
    ImGui::TableSetupColumn("Summary", ImGuiTableColumnFlags_WidthStretch, 0.66f);
    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 104.f);
    ImGui::TableNextRow();
    return true;
}

inline void EndRuleHeaderTable() { ImGui::EndTable(); }

inline void CenterCursorForWidth(float width) {
    const float avail = ImGui::GetContentRegionAvail().x;
    if (avail > width) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
}

inline void CenterCursorForCell(float width, float height) {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x > width) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - width) * 0.5f);
    (void)height;
}

inline bool DrawTrashButton(const char* id, const ImVec2& size) {
    if (ImGui::InvisibleButton(id, size)) return true;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImU32 bg = active ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
                            : hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                                      : ImGui::GetColorU32(ImGuiCol_Button);
    const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 fg = ImGui::GetColorU32(ImGuiCol_Text);

    dl->AddRectFilled(min, max, bg, 4.f);
    dl->AddRect(min, max, border, 4.f);

    const float w = max.x - min.x;
    const float h = max.y - min.y;
    const float cx = min.x + w * 0.5f;
    const float lidY = min.y + h * 0.34f;
    const float bodyTop = min.y + h * 0.42f;
    const float bodyBottom = min.y + h * 0.76f;
    const float halfBody = w * 0.18f;

    dl->AddLine(ImVec2(cx - halfBody - 1.f, lidY), ImVec2(cx + halfBody + 1.f, lidY), fg, 1.4f);
    dl->AddLine(ImVec2(cx - w * 0.08f, lidY - h * 0.06f), ImVec2(cx + w * 0.08f, lidY - h * 0.06f),
                fg, 1.2f);
    dl->AddRect(ImVec2(cx - halfBody, bodyTop), ImVec2(cx + halfBody, bodyBottom), fg, 1.5f, 0, 1.2f);
    dl->AddLine(ImVec2(cx - w * 0.07f, bodyTop + 1.f), ImVec2(cx - w * 0.07f, bodyBottom - 1.f), fg, 1.f);
    dl->AddLine(ImVec2(cx, bodyTop + 1.f), ImVec2(cx, bodyBottom - 1.f), fg, 1.f);
    dl->AddLine(ImVec2(cx + w * 0.07f, bodyTop + 1.f), ImVec2(cx + w * 0.07f, bodyBottom - 1.f), fg, 1.f);
    return false;
}

inline bool DrawEditButton(const char* id, const ImVec2& size) {
    if (ImGui::InvisibleButton(id, size)) return true;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImU32 bg = active ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
                            : hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                                      : ImGui::GetColorU32(ImGuiCol_Button);
    const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 fg = ImGui::GetColorU32(ImGuiCol_Text);

    dl->AddRectFilled(min, max, bg, 4.f);
    dl->AddRect(min, max, border, 4.f);

    const float w = max.x - min.x;
    const float h = max.y - min.y;
    const ImVec2 p0(min.x + w * 0.30f, min.y + h * 0.68f);
    const ImVec2 p1(min.x + w * 0.66f, min.y + h * 0.32f);
    const ImVec2 tip(min.x + w * 0.74f, min.y + h * 0.24f);
    const float thick = 1.7f;

    dl->AddLine(p0, p1, fg, thick);
    dl->AddLine(ImVec2(p0.x - 1.5f, p0.y + 4.f), ImVec2(p0.x + 4.f, p0.y + 1.5f), fg, thick - 0.3f);
    dl->AddLine(p1, tip, fg, thick);
    dl->AddLine(ImVec2(tip.x - 3.f, tip.y + 1.f), ImVec2(tip.x - 1.f, tip.y + 3.f), fg, 1.2f);
    return false;
}

inline void DrawRuleActionButtons(bool canMoveUp, bool canMoveDown, bool showDelete,
                                  bool& moveUp, bool& moveDown, bool& remove) {
    const ImVec2 buttonSize(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());
    const float totalWidth = showDelete ? buttonSize.x * 3.f + 16.f : buttonSize.x * 2.f + 8.f;
    CenterCursorForWidth(totalWidth);
    ImGui::BeginDisabled(!canMoveUp);
    if (ImGui::ArrowButton("##up", ImGuiDir_Up)) moveUp = true;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!canMoveDown);
    if (ImGui::ArrowButton("##down", ImGuiDir_Down)) moveDown = true;
    ImGui::EndDisabled();
    if (showDelete) {
        ImGui::SameLine();
        if (DrawTrashButton("##delete", buttonSize)) remove = true;
    }
}

inline void DrawRuleColorSourceCombo(const char* id, bool& useRuneshapeColor) {
    const char* preview = useRuneshapeColor ? "Runeshape if available" : "Static";
    if (!ImGui::BeginCombo(id, preview, ImGuiComboFlags_HeightLargest)) return;
    if (ImGui::Selectable("Static", !useRuneshapeColor)) useRuneshapeColor = false;
    if (ImGui::Selectable("Runeshape if available", useRuneshapeColor)) useRuneshapeColor = true;
    ImGui::EndCombo();
}

inline void DrawDisplayRuleStyleRow(bool hideEditable, bool& hideValue,
                                    RadarData::MarkerShape& shape, RadarData::Rgba8& color,
                                    float& size, bool labelEditable, std::string& label,
                                    bool showRuneshapeColorSource = false,
                                    bool* useRuneshapeColor = nullptr) {
    ImGui::BeginDisabled(!hideEditable);
    ImGui::Checkbox("Hide", &hideValue);
    ImGui::EndDisabled();
    ImGui::SameLine();
    DrawInlineRuleLabel("Marker");
    ImGui::SetNextItemWidth(132.f);
    DrawMarkerShapeCombo("##Marker", shape);
    ImGui::SameLine();
    if (showRuneshapeColorSource && useRuneshapeColor) {
        DrawInlineRuleLabel("Colour Source");
        ImGui::SetNextItemWidth(168.f);
        DrawRuleColorSourceCombo("##ColorSource", *useRuneshapeColor);
        ImGui::SameLine();
    }
    DrawInlineRuleLabel("Color");
    ImVec4 colorEdit = color.ToImVec4();
    ImGui::SetNextItemWidth(72.f);
    if (ImGui::ColorEdit4("##Color", &colorEdit.x,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
        color = RadarData::Rgba8::FromImVec4(colorEdit);
    ImGui::SameLine();
    DrawInlineRuleLabel("Opacity");
    DrawRuleOpacitySlider("##Opacity", color, 82.f);
    ImGui::SameLine();
    DrawInlineRuleLabel("Size");
    ImGui::SetNextItemWidth(66.f);
    ImGui::DragFloat("##Size", &size, 0.1f, 0.5f, 80.f, "%.1f");
    ImGui::SameLine();
    DrawInlineRuleLabel("Label");
    char labelBuf[256]{};
    strncpy_s(labelBuf, label.c_str(), sizeof(labelBuf) - 1);
    ImGui::BeginDisabled(!labelEditable);
    ImGui::SetNextItemWidth(190.f);
    if (ImGui::InputTextWithHint("##Label", "label (optional)", labelBuf, sizeof(labelBuf)))
        label = labelBuf;
    ImGui::EndDisabled();
}

inline RadarData::IconDef* ResolveBuiltinRuleDef(RadarData::IconTables& icons,
                                                 const BuiltinRuleRef& ref) {
    auto lookup = [&](auto& map) -> RadarData::IconDef* {
        auto it = map.find(ref.key);
        return it == map.end() ? nullptr : &it->second;
    };
    switch (ref.group) {
        case BuiltinRuleGroup::Base:
            return lookup(icons.baseIcons);
        case BuiltinRuleGroup::Chest:
            return lookup(icons.chestIcons);
        case BuiltinRuleGroup::Breach:
            return lookup(icons.breachIcons);
        case BuiltinRuleGroup::Delirium:
            return lookup(icons.deliriumIcons);
        case BuiltinRuleGroup::Expedition:
            return lookup(icons.expeditionIcons);
        default:
            return nullptr;
    }
}

inline void DrawBuiltInRuleRow(const BuiltinRuleRef& ref, RadarData::IconTables& icons) {
    RadarData::IconDef* def = ResolveBuiltinRuleDef(icons, ref);
    if (!def) return;

    ImGui::PushID(ref.key);
    PushRuleCardStyle();
    bool open = false;
    bool enabled = def->scale > 0.f;
    bool moveUp = false;
    bool moveDown = false;
    bool remove = false;
    if (BeginRuleHeaderTable("##syshdr")) {
        ImGui::TableSetColumnIndex(0);
        open = ImGui::TreeNodeEx("##sysrule",
                                 ImGuiTreeNodeFlags_SpanFullWidth
                                     | ImGuiTreeNodeFlags_FramePadding,
                                 "");
        ImGui::TableSetColumnIndex(1);
        CenterCursorForCell(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());
        if (ImGui::Checkbox("##enabled", &enabled)) {
            if (!enabled) def->scale = 0.f;
            else if (def->scale <= 0.f) def->scale = 30.f;
        }
        ImGui::TableSetColumnIndex(2);
        DrawRuleMarkerPreview(def->markerShape, def->markerColor);
        ImGui::TableSetColumnIndex(3);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(ref.title);
        ImGui::TableSetColumnIndex(4);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", ref.summary);
        ImGui::TableSetColumnIndex(5);
        DrawRuleActionButtons(false, false, false, moveUp, moveDown, remove);
        EndRuleHeaderTable();
    }
    if (open) {
        RadarData::DisplayRule preview = BuildBuiltinRuleTemplate(ref);
        preview.markerShape = def->markerShape;
        preview.markerColor = def->markerColor;
        preview.size = def->scale;
        preview.label = def->label;

        DrawDisplayRuleMatcherFields(preview, false, true, "##sys");

        bool hide = false;
        DrawDisplayRuleStyleRow(false, hide, def->markerShape, def->markerColor, def->scale,
                                true, def->label);
        ImGui::TreePop();
    }
    PopRuleCardStyle();
    ImGui::PopID();
}

inline void DrawDisplayRuleRow(size_t index, RadarData::IconTables& icons, RuleSection section) {
    auto& rule = icons.displayRules[index];
    ImGui::PushID(static_cast<int>(index));
    PushRuleCardStyle();
    bool open = false;
    bool moveUp = false;
    bool moveDown = false;
    bool remove = false;
    const bool canMoveUp = FindPrevRuleInSection(icons.displayRules, index, section).has_value();
    const bool canMoveDown = FindNextRuleInSection(icons.displayRules, index, section).has_value();
    if (BeginRuleHeaderTable("##customhdr")) {
        ImGui::TableSetColumnIndex(0);
        open = ImGui::TreeNodeEx("##customrule",
                                 ImGuiTreeNodeFlags_SpanFullWidth
                                     | ImGuiTreeNodeFlags_FramePadding,
                                 "");
        ImGui::TableSetColumnIndex(1);
        CenterCursorForCell(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());
        ImGui::Checkbox("##enabled", &rule.enabled);
        ImGui::TableSetColumnIndex(2);
        DrawRuleMarkerPreview(rule.markerShape, rule.markerColor);
        ImGui::TableSetColumnIndex(3);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(rule.name.empty() ? "New rule" : rule.name.c_str());
        ImGui::TableSetColumnIndex(4);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", DisplayRuleSummary(rule).c_str());
        ImGui::TableSetColumnIndex(5);
        ImGui::PushButtonRepeat(true);
        DrawRuleActionButtons(canMoveUp, canMoveDown, true, moveUp, moveDown, remove);
        ImGui::PopButtonRepeat();
        EndRuleHeaderTable();
    }
    if (moveUp) {
        if (const auto prev = FindPrevRuleInSection(icons.displayRules, index, section))
            std::swap(icons.displayRules[index], icons.displayRules[*prev]);
    }
    if (moveDown) {
        if (const auto next = FindNextRuleInSection(icons.displayRules, index, section))
            std::swap(icons.displayRules[index], icons.displayRules[*next]);
    }
    if (remove) {
        icons.displayRules.erase(icons.displayRules.begin() + static_cast<std::ptrdiff_t>(index));
        PopRuleCardStyle();
        ImGui::PopID();
        return;
    }
    if (open) {
        DrawDisplayRuleMatcherFields(rule, true, true, "##cat");
        const bool runeshapeEligible = RadarData::IsRuneshapeColourEligible(rule);
        DrawDisplayRuleStyleRow(true, rule.hide, rule.markerShape, rule.markerColor, rule.size,
                                true, rule.label, runeshapeEligible, &rule.useRuneshapeColor);
        ImGui::TreePop();
    }
    PopRuleCardStyle();
    ImGui::PopID();
}

inline void DrawRulePicker(UiState& ui, PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                           RadarData::IconTables& icons) {
    if (!ui.rulePickerOpen) return;
    ImGui::SetNextWindowSize(ImVec2(760.f, 520.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Add Rule From Game Data", &ui.rulePickerOpen)) {
        ImGui::End();
        return;
    }

    ImGui::InputTextWithHint("##ruleSearch", "filter by name / metadata ...",
                              ui.rulePickerSearch, IM_ARRAYSIZE(ui.rulePickerSearch));
    static constexpr const char* kKinds[] = {"All", "Entities", "Tiles", "Mods"};
    for (int i = 0; i < static_cast<int>(sizeof(kKinds) / sizeof(kKinds[0])); ++i) {
        if (i != 0) ImGui::SameLine();
        if (ImGui::RadioButton(kKinds[i], ui.rulePickerCategory == i))
            ui.rulePickerCategory = i;
    }
    ImGui::Separator();

    const std::string query = ui.rulePickerSearch;
    std::vector<RulePickerRow> rows;
    rows.reserve(snap.Entities.size());

    std::unordered_set<std::string> seenEntities;
    std::unordered_set<std::string> seenTiles;
    std::unordered_map<std::string, RulePickerRow> modRows;

    auto queryMatches = [&](std::string_view a, std::string_view b = {},
                            std::string_view c = {}) {
        if (query.empty()) return true;
        return ContainsCaseInsensitiveUi(a, query) || ContainsCaseInsensitiveUi(b, query)
               || ContainsCaseInsensitiveUi(c, query);
    };

    for (const auto& e : snap.Entities) {
        if (!e.IsValid) continue;
        const std::string path(e.Path.begin(), e.Path.end());
        if (path.empty()) continue;
        const std::string key = std::string(DisplayRuleCategoryForEntity(e)) + "|" + path;
        if (!seenEntities.insert(key).second) continue;

        RulePickerRow row;
        row.kind = RulePickerRow::Kind::Entity;
        row.category = DisplayRuleCategoryForEntity(e);
        row.name = BaseNameForPath(path);
        row.detail = path;
        row.seedValue = path;
        rows.push_back(std::move(row));

        if (ctx && e.Components.OMP != 0) {
            for (const auto& mod : ctx->Components.EnumerateMonsterMods(e.Components.OMP)) {
                if (mod.Id.empty()) continue;
                auto& rowRef = modRows[mod.Id];
                rowRef.kind = RulePickerRow::Kind::Mod;
                rowRef.category = "Mod";
                rowRef.name = mod.Id;
                rowRef.seedValue = mod.Id;
                if (!mod.Name.empty()) rowRef.detail = mod.Name;
                else if (!mod.Metadata.empty()) rowRef.detail = mod.Metadata;
            }
        }
    }

    if (ctx) {
        ctx->Terrain.EnumerateTgtLocations([&](const PluginSDK::TgtLocation& loc) {
            if (loc.Path.empty()) return true;
            if (!seenTiles.insert(loc.Path).second) return true;
            RulePickerRow row;
            row.kind = RulePickerRow::Kind::Tile;
            row.category = "Tile";
            row.name = BaseNameForPath(loc.Path);
            row.detail = loc.Path;
            row.seedValue = loc.Path;
            rows.push_back(std::move(row));
            return true;
        });
    }

    for (auto& [_, row] : modRows) rows.push_back(std::move(row));

    ImGui::BeginChild("##rulePickList", ImVec2(0, 0), true);
    for (const auto& row : rows) {
        const bool kindFiltered =
            ui.rulePickerCategory == 0
            || (ui.rulePickerCategory == 1 && row.kind == RulePickerRow::Kind::Entity)
            || (ui.rulePickerCategory == 2 && row.kind == RulePickerRow::Kind::Tile)
            || (ui.rulePickerCategory == 3 && row.kind == RulePickerRow::Kind::Mod);
        if (!kindFiltered) continue;
        if (!queryMatches(row.name, row.detail, row.category)) continue;

        ImGui::PushID(row.seedValue.c_str());
        if (ImGui::Selectable((row.name + "##pick").c_str(), false)) {
            if (row.kind == RulePickerRow::Kind::Entity) {
                for (const auto& e : snap.Entities) {
                    const std::string path(e.Path.begin(), e.Path.end());
                    if (!e.IsValid || path != row.seedValue) continue;
                    icons.displayRules.insert(
                        icons.displayRules.begin()
                            + static_cast<std::ptrdiff_t>(UserRuleInsertIndex(icons.displayRules)),
                        SeedRuleFromEntity(e));
                    break;
                }
            } else if (row.kind == RulePickerRow::Kind::Tile) {
                icons.displayRules.insert(
                    icons.displayRules.begin()
                        + static_cast<std::ptrdiff_t>(UserRuleInsertIndex(icons.displayRules)),
                    SeedRuleFromTilePath(row.seedValue));
            } else {
                icons.displayRules.insert(
                    icons.displayRules.begin()
                        + static_cast<std::ptrdiff_t>(UserRuleInsertIndex(icons.displayRules)),
                    SeedRuleFromMonsterMod(row.seedValue));
            }
            ui.rulePickerOpen = false;
            ImGui::PopID();
            break;
        }
        ImGui::SameLine(260.f);
        ImGui::TextDisabled("%s", row.category.c_str());
        ImGui::SameLine(340.f);
        ImGui::TextUnformatted(row.detail.c_str());
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

inline void DrawDisplayRuleSection(const char* label, RuleSection section,
                                   RadarData::IconTables& icons) {
    if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) return;
    bool any = false;
    for (size_t i = 0; i < icons.displayRules.size();) {
        if (GetRuleSection(icons.displayRules[i]) != section) {
            ++i;
            continue;
        }
        any = true;
        const size_t before = icons.displayRules.size();
        DrawDisplayRuleRow(i, icons, section);
        if (icons.displayRules.size() < before) continue;
        ++i;
    }
    if (!any) ImGui::TextDisabled("No rules in this section.");
}

inline void DrawIconsTab(RadarData::RadarConfig& cfg, RadarData::IconTables& icons, UiState& ui,
                         RadarRender::IconAtlas&, PluginSDK::Context* ctx,
                         const PluginSDK::Snapshot& snap) {
    ImGui::Checkbox("Edge Indicators (Minimap)", &cfg.EdgeIndicatorMinimap);
    ImGui::SameLine();
    ImGui::Checkbox("Edge Indicators (Large Map)", &cfg.EdgeIndicatorLargemap);
    ImGui::TextWrapped("Display rules are evaluated top-down. Custom rules can match category, "
                       "metadata, monster mods, rarity, reaction, life, chest state, subtype, state, "
                       "and minimap POI.");
    if (ImGui::Button("+ Add from game data...", ImVec2(0, 0))) {
        ui.rulePickerOpen = true;
        ui.rulePickerCategory = 0;
        ui.rulePickerSearch[0] = '\0';
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Add blank rule", ImVec2(0, 0))) {
        RadarData::DisplayRule rule;
        rule.source = "User";
        icons.displayRules.insert(icons.displayRules.begin() + static_cast<std::ptrdiff_t>(UserRuleInsertIndex(icons.displayRules)),
                                  rule);
    }
    ImGui::SameLine();
    if (ImGui::Button("Restore seeded defaults", ImVec2(0, 0))) {
        RadarData::IconTables::ReplaceSeededDefaults(icons.displayRules);
    }
    ImGui::Separator();

    ImGui::BeginChild("##ruleScroll", ImVec2(0, 0), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    DrawDisplayRuleSection("State Hides", RuleSection::StateHides, icons);
    DrawDisplayRuleSection("User Rules", RuleSection::User, icons);
    DrawDisplayRuleSection("Seeded Defaults", RuleSection::Seeded, icons);
    ImGui::EndChild();
}

inline void PushPoiActionButtonStyle() {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.32f, 0.58f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.40f, 0.68f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.28f, 0.50f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
}

inline void PopPoiActionButtonStyle() { ImGui::PopStyleColor(4); }

inline bool DrawPoiCategoryHeader(const char* id, const char* label, bool defaultOpen) {
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.28f, 0.52f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.14f, 0.34f, 0.60f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.08f, 0.24f, 0.46f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
                               | ImGuiTreeNodeFlags_AllowOverlap;
    if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    const bool open = ImGui::TreeNodeEx(label, flags);
    ImGui::PopStyleColor(4);
    ImGui::PopID();
    return open;
}

inline void DrawTargetIndicesTable(RadarData::TargetDatabase& db,
                                   const std::vector<size_t>& indices,
                                   const std::string& areaKey, UiState& ui,
                                   RadarRender::RadarOverlay& overlay,
                                   RadarRender::IconAtlas& atlas,
                                   const std::filesystem::path& pluginDir,
                                   bool userOnly = false) {
    if (indices.empty()) return;

    size_t visibleCount = 0;
    for (size_t idx : indices) {
        if (idx >= db.storage.size()) continue;
        if (userOnly && db.storage[idx].category != "User") continue;
        ++visibleCount;
    }
    if (visibleCount == 0) return;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.f, 2.f));
    ImGui::PushID(areaKey.c_str());
    const ImGuiTableFlags tblFlags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg
                                     | ImGuiTableFlags_SizingFixedFit
                                     | ImGuiTableFlags_NoHostExtendX;
    if (!ImGui::BeginTable("poi", 8, tblFlags)) {
        ImGui::PopID();
        ImGui::PopStyleVar();
        return;
    }
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 24.f);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 150.f);
    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed, 620.f);
    ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed, 36.f);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 36.f);
    ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 40.f);
    ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed, 30.f);
    ImGui::TableSetupColumn("Del", ImGuiTableColumnFlags_WidthFixed, 30.f);
    ImGui::TableHeadersRow();

    for (size_t idx : indices) {
        if (idx >= db.storage.size()) continue;
        auto& t = db.storage[idx];
        if (userOnly && t.category != "User") continue;
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        bool enabled = t.enabled;
        if (ImGui::Checkbox(("##en" + std::to_string(idx)).c_str(), &enabled)) {
            t.enabled = enabled;
            overlay.cache.InvalidatePoi();
            if (t.category == "User") db.SaveUser(pluginDir);
        }
        ImGui::TableSetColumnIndex(1);
        if (t.category == "User") {
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.f, 1.f), "%s", t.name.c_str());
        } else {
            ImGui::TextUnformatted(t.name.c_str());
        }
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(t.path.c_str());
        ImGui::TableSetColumnIndex(3);
        if (t.showIcon && atlas.Valid()) {
            const auto iconDef = RadarRender::PoiDrawCache::ResolvePoiIcon(t, overlay.icons);
            ImVec2 p = ImGui::GetCursorScreenPos();
            atlas.DrawIcon(ImGui::GetWindowDrawList(), iconDef.cx, iconDef.cy, 18.f,
                           p.x + 12.f, p.y + 10.f);
        }
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.0f", t.iconSize);
        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%d", t.expectedCount);
        ImGui::TableSetColumnIndex(6);
        ImGui::PushID(static_cast<int>(idx * 2));
        PushPoiActionButtonStyle();
        CenterCursorForCell(22.f, 20.f);
        if (DrawEditButton("##edit", ImVec2(22.f, 20.f))) {
            ui.editTarget = t;
            ui.editAreaKey = areaKey;
            ui.editIsNew = false;
            ui.editStorageIndex = idx;
            ui.editModalOpen = true;
        }
        PopPoiActionButtonStyle();
        ImGui::PopID();
        ImGui::TableSetColumnIndex(7);
        ImGui::PushID(static_cast<int>(idx * 2 + 1));
        PushPoiActionButtonStyle();
        CenterCursorForCell(22.f, 20.f);
        if (DrawTrashButton("##delete", ImVec2(22.f, 20.f))) {
            if (t.category == "User") {
                db.RemoveUserTargetFromArea(idx, areaKey);
                db.SaveUser(pluginDir);
            } else {
                t.enabled = false;
            }
            overlay.cache.InvalidatePoi();
        }
        PopPoiActionButtonStyle();
        ImGui::PopID();
    }
    ImGui::EndTable();
    ImGui::PopID();
    ImGui::PopStyleVar();
}

inline void DrawAreaTargetTable(RadarData::TargetDatabase& db, const std::string& areaKey,
                                UiState& ui, RadarRender::RadarOverlay& overlay,
                                RadarRender::IconAtlas& atlas,
                                const std::filesystem::path& pluginDir,
                                bool userOnly = false) {
    const std::string key = RadarData::NormalizeAreaKey(areaKey);
    if (key == "*" || key == "GLOBAL") {
        DrawTargetIndicesTable(db, db.actsGlobalTargets, key, ui, overlay, atlas, pluginDir, userOnly);
        return;
    }
    auto it = db.byArea.find(key);
    if (it == db.byArea.end()) return;
    DrawTargetIndicesTable(db, it->second, key, ui, overlay, atlas, pluginDir, userOnly);
}

inline bool DrawAreaSubNode(RadarData::TargetDatabase& db, const std::string& areaKey,
                            const std::string& label, bool isCurrent, bool defaultOpen,
                            UiState& ui, RadarRender::RadarOverlay& overlay,
                            const std::filesystem::path& pluginDir,
                            bool userOnly = false) {
    ImGui::PushID(areaKey.c_str());
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.92f, 0.35f, 1.f));
    const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
    if (isCurrent) ImGui::PopStyleColor();
    if (open) {
        DrawAreaTargetTable(db, areaKey, ui, overlay, overlay.atlas, pluginDir, userOnly);
        ImGui::TreePop();
    }
    ImGui::PopID();
    return open;
}

inline std::string ResolveCurrentAreaKey(const PluginSDK::Snapshot& snap,
                                         const RadarData::TargetDatabase& db) {
    return db.ResolveAreaKey(snap.CurrentAreaHash, snap.CurrentAreaName);
}

// Campaign custom areas (User) appear under Acts; maps under Endgame.
inline std::string ResolveCurrentAreaSection(const RadarData::TargetDatabase& db,
                                             const std::string& currentArea) {
    if (currentArea.empty()) return "Acts";
    const std::string key = RadarData::NormalizeAreaKey(currentArea);
    if (auto it = db.areaSource.find(key); it != db.areaSource.end()) {
        if (it->second == "Endgame") return "Endgame";
        return "Acts";
    }
    if (key.size() >= 3
        && (key.rfind("MAP", 0) == 0 || key.rfind("SANCTUM", 0) == 0))
        return "Endgame";
    return "Acts";
}

inline std::vector<std::string> CollectAreasForSection(const RadarData::TargetDatabase& db,
                                                       const std::string& sourceKey) {
    std::vector<std::string> areas = db.ListAreas(sourceKey);
    if (sourceKey != "Acts") return areas;

    for (const std::string& ua : db.ListUserAreas()) {
        const bool already = std::any_of(areas.begin(), areas.end(),
                                         [&](const std::string& a) {
                                             return RadarData::AreaKeysEqual(a, ua);
                                         });
        if (!already) areas.push_back(ua);
    }
    std::sort(areas.begin(), areas.end());
    return areas;
}

inline void DrawAreaTreeSection(RadarData::TargetDatabase& db, const std::string& sourceLabel,
                                const std::string& sourceKey, const std::string& currentArea,
                                bool pinCurrentFirst, UiState& ui,
                                RadarRender::RadarOverlay& overlay,
                                const std::filesystem::path& pluginDir) {
    auto areas = CollectAreasForSection(db, sourceKey);
    const bool hasGlobal = !db.actsGlobalTargets.empty() && sourceKey == "Acts";
    const bool hasCurrentBucket =
        !currentArea.empty() && pinCurrentFirst
        && (db.HasAreaBucket(currentArea)
            || db.byArea.find(RadarData::NormalizeAreaKey(currentArea)) != db.byArea.end());
    if (areas.empty() && !hasGlobal && !hasCurrentBucket) return;

    const bool sectionOpen =
        DrawPoiCategoryHeader(sourceKey.c_str(), sourceLabel.c_str(), sourceKey == "Acts");
    if (!sectionOpen) return;

    if (hasGlobal && sourceKey == "Acts") {
        DrawAreaSubNode(db, "*", "Global", false, true, ui, overlay, pluginDir);
    }

    auto drawAreaNode = [&](const std::string& area, bool isCurrent) {
        std::string label = db.DisplayNameForArea(area);
        if (isCurrent) label += " [Current]";
        DrawAreaSubNode(db, area, label, isCurrent, isCurrent, ui, overlay, pluginDir);
    };

    if (hasCurrentBucket) {
        const std::string key = RadarData::NormalizeAreaKey(currentArea);
        const bool inList = std::any_of(areas.begin(), areas.end(),
                                        [&](const std::string& a) {
                                            return RadarData::AreaKeysEqual(a, key);
                                        });
        if (!inList) areas.insert(areas.begin(), key);
        drawAreaNode(key, true);
    }

    for (const std::string& area : areas) {
        if (hasCurrentBucket && RadarData::AreaKeysEqual(area, currentArea)) continue;
        drawAreaNode(area, false);
    }

    ImGui::TreePop();
}

inline std::vector<std::string> CollectCustomAreas(const RadarData::TargetDatabase& db) {
    std::vector<std::string> areas;
    for (const auto& [area, indices] : db.byArea) {
        bool hasUser = false;
        for (size_t idx : indices) {
            if (idx < db.storage.size() && db.storage[idx].category == "User"
                && db.storage[idx].enabled) {
                hasUser = true;
                break;
            }
        }
        if (hasUser) areas.push_back(area);
    }
    std::sort(areas.begin(), areas.end());
    areas.erase(std::unique(areas.begin(), areas.end()), areas.end());
    return areas;
}

inline void DrawCustomTreeSection(RadarData::TargetDatabase& db, const std::string& currentArea,
                                  UiState& ui, RadarRender::RadarOverlay& overlay,
                                  const std::filesystem::path& pluginDir) {
    const auto areas = CollectCustomAreas(db);
    if (areas.empty()) return;
    const bool sectionOpen = DrawPoiCategoryHeader("Custom", "Custom", false);
    if (!sectionOpen) return;
    for (const std::string& area : areas) {
        std::string label = db.DisplayNameForArea(area);
        if (RadarData::AreaKeysEqual(area, currentArea)) label += " [Current]";
        DrawAreaSubNode(db, area, label, RadarData::AreaKeysEqual(area, currentArea),
                        RadarData::AreaKeysEqual(area, currentArea), ui, overlay, pluginDir, true);
    }
    ImGui::TreePop();
}

inline void DrawEditTargetModal(UiState& ui, RadarData::TargetDatabase& db,
                                const std::filesystem::path& pluginDir,
                                RadarRender::RadarOverlay& overlay) {
    if (!ui.editModalOpen) return;
    ImGui::SetNextWindowSize(ImVec2(480, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Edit Target", &ui.editModalOpen)) {
        ImGui::End();
        return;
    }
    char nameBuf[256];
    strncpy_s(nameBuf, ui.editTarget.name.c_str(), sizeof(nameBuf) - 1);
    ImGui::InputText("Name", nameBuf, sizeof(nameBuf));
    ui.editTarget.name = nameBuf;
    char pathBuf[512];
    strncpy_s(pathBuf, ui.editTarget.path.c_str(), sizeof(pathBuf) - 1);
    ImGui::InputText("Metadata Path", pathBuf, sizeof(pathBuf));
    ui.editTarget.path = pathBuf;
    ImGui::Checkbox("Enabled", &ui.editTarget.enabled);
    ImGui::DragInt("Count", &ui.editTarget.expectedCount, 1, 1, 99);
    ImGui::Checkbox("Show as Icon", &ui.editTarget.showIcon);
    if (ui.editTarget.showIcon) ImGui::DragFloat("Icon Size", &ui.editTarget.iconSize, 1, 5, 80);
    if (ImGui::Button("Save", ImVec2(90, 0))) {
        const bool wasNew = ui.editIsNew;
        if (ui.editIsNew) {
            db.AddUserTarget(ui.editAreaKey, ui.editTarget);
        } else if (ui.editStorageIndex < db.storage.size()) {
            db.storage[ui.editStorageIndex] = ui.editTarget;
        }
        if (wasNew || ui.editTarget.category == "User") db.SaveUser(pluginDir);
        overlay.cache.InvalidatePoi();
        ui.editModalOpen = false;
        ui.editStorageIndex = SIZE_MAX;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(90, 0))) ui.editModalOpen = false;
    ImGui::End();
}

inline void DrawLandmarksTab(RadarData::RadarConfig& cfg, RadarData::TargetDatabase& db,
                             UiState& ui, const PluginSDK::Snapshot& snap,
                             const std::filesystem::path& pluginDir,
                             RadarRender::RadarOverlay& overlay) {
    if (ImGui::Checkbox("Landmarks / POI", &cfg.ShowImportantPOI)) {
        overlay.cache.InvalidatePoi();
        if (!cfg.ShowImportantPOI) overlay.cache.pois.Clear();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Show or hide curated landmarks and custom picked map targets on the overlay.");
    ImGui::SameLine(0.f, 8.f);
    ImGui::Checkbox("Text Background", &cfg.EnablePOIBackground);

    if (ImGui::Button("Add Landmark from Map", ImVec2(0, 0))) {
        ui.pickerPoiMode = true;
        ui.pickerEntityMode = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Entity from Map", ImVec2(0, 0))) {
        ui.pickerEntityMode = true;
        ui.pickerPoiMode = false;
    }

    const std::string currentArea = ResolveCurrentAreaKey(snap, db);
    const std::string currentSection = ResolveCurrentAreaSection(db, currentArea);

    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 2.f));
    ImGui::BeginChild("##poiTree", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    DrawCustomTreeSection(db, currentArea, ui, overlay, pluginDir);
    DrawAreaTreeSection(db, "Endgame", "Endgame", currentArea, currentSection == "Endgame",
                        ui, overlay, pluginDir);
    DrawAreaTreeSection(db, "Acts (Level < 65)", "Acts", currentArea, currentSection != "Endgame",
                        ui, overlay, pluginDir);
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    DrawEditTargetModal(ui, db, pluginDir, overlay);
}

inline void DrawSettings(RadarRender::RadarOverlay& overlay, UiState& ui,
                         PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                         const std::filesystem::path& pluginDir) {
    if (ImGui::BeginTabBar("RadarTabs")) {
        if (ImGui::BeginTabItem("General Settings")) {
            DrawGeneralTab(overlay.cfg, ui);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Display Rules")) {
            DrawIconsTab(overlay.cfg, overlay.icons, ui, overlay.atlas, ctx, snap);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Landmarks")) {
            DrawLandmarksTab(overlay.cfg, overlay.targets, ui, snap, pluginDir, overlay);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    DrawShapePicker(ui);
    DrawRulePicker(ui, ctx, snap, overlay.icons);
}

} // namespace RadarUi
