#pragma once

#include "sdk/PluginSDK.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace RadarUi {

struct UiDiscoveryCandidate {
    std::string rootLabel;
    std::string path;
    std::string stringId;
    std::string parentStringId;
    uintptr_t   address = 0;
    uintptr_t   parentAddress = 0;
    float       rectX = 0.f;
    float       rectY = 0.f;
    float       rectW = 0.f;
    float       rectH = 0.f;
    float       area = 0.f;
    int         childCount = 0;
    uint16_t    elementType = 0;
    uint32_t    flags = 0;
    uint8_t     scaleIndex = 0;
    bool        visible = false;
};

struct UiDiscoveryState {
    bool autoRefresh = false;
    bool requestedRefresh = false;
    int  scanIntervalMs = 500;

    std::vector<UiDiscoveryCandidate> candidates;
    size_t lastNodesVisited = 0;
    size_t lastVisibleRects = 0;
    bool   lastScanFailed = false;
    std::string lastError;
    std::chrono::steady_clock::time_point lastScanTime = std::chrono::steady_clock::time_point{};
};

inline const char* DiscoveryStringId(const UiDiscoveryCandidate& candidate) {
    return candidate.stringId.empty() ? "(empty)" : candidate.stringId.c_str();
}

inline std::string DiscoveryAddressText(uintptr_t address) {
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%llX", static_cast<unsigned long long>(address));
    return buf;
}

inline std::string BuildDiscoveryCopyText(const UiDiscoveryCandidate& candidate) {
    std::ostringstream out;
    out << "Root: " << candidate.rootLabel << "\n";
    out << "Path: " << candidate.path << "\n";
    out << "StringId: " << (candidate.stringId.empty() ? "(empty)" : candidate.stringId) << "\n";
    out << "Address: " << DiscoveryAddressText(candidate.address) << "\n";
    out << "ParentAddress: " << DiscoveryAddressText(candidate.parentAddress) << "\n";
    out << "ParentStringId: "
        << (candidate.parentStringId.empty() ? "(empty)" : candidate.parentStringId) << "\n";
    out << "Visible: " << (candidate.visible ? "true" : "false") << "\n";
    out << "Rect: x=" << candidate.rectX << " y=" << candidate.rectY << " w=" << candidate.rectW
        << " h=" << candidate.rectH << "\n";
    out << "Area: " << candidate.area << "\n";
    out << "ChildCount: " << candidate.childCount << "\n";
    out << "ElementType: 0x" << std::hex << candidate.elementType << std::dec << " ("
        << candidate.elementType << ")\n";
    out << "Flags: 0x" << std::hex << candidate.flags << std::dec << "\n";
    out << "ScaleIndex: " << static_cast<unsigned int>(candidate.scaleIndex) << "\n";
    return out.str();
}

inline void DrawDiscoveryHighlight(PluginSDK::Context* ctx, const UiDiscoveryCandidate& candidate) {
    if (!ctx || candidate.address == 0) return;
    float x = candidate.rectX;
    float y = candidate.rectY;
    float w = candidate.rectW;
    float h = candidate.rectH;
    if (!ctx->Ui.ComputeScreenRect(candidate.address, x, y, w, h) || w <= 0.f || h <= 0.f) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) return;
    const ImVec2 min(x, y);
    const ImVec2 max(x + w, y + h);
    dl->AddRect(min, max, IM_COL32(255, 200, 0, 255), 0.f, 0, 2.f);
    dl->AddRectFilled(min, max, IM_COL32(255, 200, 0, 24));
}

inline void RefreshUiDiscovery(UiDiscoveryState& state, PluginSDK::Context* ctx) {
    state.candidates.clear();
    state.lastNodesVisited = 0;
    state.lastVisibleRects = 0;
    state.lastScanFailed = false;
    state.lastError.clear();
    state.lastScanTime = std::chrono::steady_clock::now();

    if (!ctx) {
        state.lastScanFailed = true;
        state.lastError = "Context unavailable.";
        return;
    }

    struct QueueItem {
        uintptr_t   address = 0;
        uintptr_t   parentAddress = 0;
        std::string rootLabel;
        std::string path;
        int         depth = 0;
    };

    std::queue<QueueItem> queue;
    std::unordered_set<uintptr_t> visited;
    visited.reserve(2048);

    auto enqueueRoot = [&](uintptr_t root, const char* label) {
        if (root == 0) return;
        queue.push(QueueItem{root, 0, label, "root", 0});
    };

    const uintptr_t gameUiRoot = ctx->Ui.GetGameUiRoot();
    const uintptr_t uiRoot = ctx->Ui.GetUiRoot();
    enqueueRoot(gameUiRoot, "GameUiRoot");
    if (uiRoot != 0 && uiRoot != gameUiRoot) enqueueRoot(uiRoot, "UiRoot");

    if (queue.empty()) {
        state.lastScanFailed = true;
        state.lastError = "No UI root available.";
        return;
    }

    constexpr int kMaxDepth = 24;
    constexpr size_t kMaxVisited = 20000;
    constexpr size_t kMaxCandidates = 512;

    while (!queue.empty() && visited.size() < kMaxVisited) {
        QueueItem item = std::move(queue.front());
        queue.pop();
        if (item.address == 0 || item.depth > kMaxDepth) continue;
        if (!visited.insert(item.address).second) continue;
        state.lastNodesVisited = visited.size();

        const PluginSDK::UiElement element = ctx->Ui.Read(item.address);
        if (!element.Valid) continue;

        if (element.IsVisible) {
            float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
            if (ctx->Ui.ComputeScreenRect(item.address, x, y, w, h) && w > 0.f && h > 0.f) {
                UiDiscoveryCandidate candidate;
                candidate.rootLabel = item.rootLabel;
                candidate.path = item.path;
                candidate.stringId = ctx->Ui.GetStringId(item.address);
                candidate.parentAddress = item.parentAddress;
                if (item.parentAddress != 0)
                    candidate.parentStringId = ctx->Ui.GetStringId(item.parentAddress);
                candidate.address = item.address;
                candidate.rectX = x;
                candidate.rectY = y;
                candidate.rectW = w;
                candidate.rectH = h;
                candidate.area = w * h;
                candidate.childCount = element.ChildCount;
                candidate.elementType = element.ElementType;
                candidate.flags = element.Flags;
                candidate.scaleIndex = element.ScaleIndex;
                candidate.visible = element.IsVisible;
                state.candidates.push_back(std::move(candidate));
                state.lastVisibleRects++;
                if (state.candidates.size() >= kMaxCandidates) break;
            }
        }

        if (element.ChildCount <= 0) continue;
        const auto children = ctx->Ui.GetChildren(item.address);
        for (size_t i = 0; i < children.size(); ++i) {
            const uintptr_t child = children[i];
            if (child == 0) continue;
            std::string childPath = item.path;
            childPath.push_back('/');
            childPath += std::to_string(i);
            queue.push(QueueItem{child, item.address, item.rootLabel, std::move(childPath),
                                 item.depth + 1});
        }
    }

    std::sort(state.candidates.begin(), state.candidates.end(),
              [](const UiDiscoveryCandidate& a, const UiDiscoveryCandidate& b) {
                  if (a.area != b.area) return a.area > b.area;
                  if (a.childCount != b.childCount) return a.childCount > b.childCount;
                  return a.address < b.address;
              });
}

inline void DrawUiDiscoverySection(UiDiscoveryState& state, PluginSDK::Context* ctx) {
    if (!ImGui::CollapsingHeader("UI Discovery", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Indent(12.f);
    if (ImGui::Button("Refresh UI Candidates", ImVec2(180.f, 0.f))) state.requestedRefresh = true;
    ImGui::SameLine();
    ImGui::Checkbox("Auto refresh", &state.autoRefresh);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.f);
    ImGui::SliderInt("Interval ms", &state.scanIntervalMs, 250, 2000, "%d");

    const auto now = std::chrono::steady_clock::now();
    const bool intervalElapsed = state.lastScanTime.time_since_epoch().count() == 0
                                 || now - state.lastScanTime
                                        >= std::chrono::milliseconds(state.scanIntervalMs);
    if (state.requestedRefresh || (state.autoRefresh && intervalElapsed)) {
        state.requestedRefresh = false;
        RefreshUiDiscovery(state, ctx);
    }

    if (state.lastScanTime.time_since_epoch().count() != 0) {
        const auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.lastScanTime).count();
        ImGui::Text("Last scan age: %lld ms", static_cast<long long>(ageMs));
    } else {
        ImGui::TextDisabled("No UI discovery scan yet.");
    }
    ImGui::Text("Visited nodes: %zu", state.lastNodesVisited);
    ImGui::Text("Visible rect candidates: %zu", state.lastVisibleRects);
    ImGui::Text("Stored candidates: %zu", state.candidates.size());
    if (state.lastScanFailed) {
        ImGui::TextColored(ImVec4(1.f, 0.45f, 0.45f, 1.f), "Scan failed: %s",
                           state.lastError.c_str());
    }

    if (!state.candidates.empty()) {
        ImGui::BeginChild("##UiDiscoveryCandidates", ImVec2(0.f, 320.f), true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        const size_t limit = std::min<size_t>(state.candidates.size(), 30);
        for (size_t i = 0; i < limit; ++i) {
            const auto& candidate = state.candidates[i];
            const std::string header = candidate.rootLabel + " " + candidate.path + " | "
                                       + DiscoveryStringId(candidate) + " | "
                                       + DiscoveryAddressText(candidate.address);
            const bool selected = ImGui::Selectable(header.c_str(), false);
            if (selected) {
                const std::string copyText = BuildDiscoveryCopyText(candidate);
                ImGui::SetClipboardText(copyText.c_str());
            }
            if (ImGui::IsItemHovered()) DrawDiscoveryHighlight(ctx, candidate);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.72f, 0.72f, 1.f));
            ImGui::TextWrapped(
                "rect=(%.0f, %.0f %.0fx%.0f) area=%.0f children=%d parent=%s %s type=0x%04X flags=0x%08X scale=%u",
                candidate.rectX, candidate.rectY, candidate.rectW, candidate.rectH, candidate.area,
                candidate.childCount,
                candidate.parentStringId.empty() ? "(empty)" : candidate.parentStringId.c_str(),
                DiscoveryAddressText(candidate.parentAddress).c_str(), candidate.elementType,
                candidate.flags, static_cast<unsigned int>(candidate.scaleIndex));
            ImGui::PopStyleColor();
            ImGui::Separator();
        }
        ImGui::EndChild();
    }

    ImGui::Unindent(12.f);
}

} // namespace RadarUi
