#pragma once

#include "sdk/PluginSDK.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace RadarUi {

inline constexpr const char* kDiscoveryRootLabels[] = {"GameUiRoot", "UiRoot", "TopUiRoot"};

struct UiDiscoveryNode {
    std::string rootLabel;
    std::string path;
    std::string parentPath;
    std::string sdkStringId;
    std::string rawStringId;
    std::string parentSdkStringId;
    std::string parentRawStringId;
    uintptr_t   address = 0;
    uintptr_t   parentAddress = 0;
    float       rectX = 0.f;
    float       rectY = 0.f;
    float       rectW = 0.f;
    float       rectH = 0.f;
    float       area = 0.f;
    int         childCount = 0;
    int         depth = 0;
    uint16_t    elementType = 0;
    uint32_t    flags = 0;
    uint8_t     scaleIndex = 0;
    bool        visible = false;
    bool        hasRect = false;
};

enum class UiDiscoveryDiffKind : uint8_t {
    Changed = 0,
    Appeared,
    Disappeared,
};

struct UiDiscoveryDiffRow {
    UiDiscoveryDiffKind    kind = UiDiscoveryDiffKind::Changed;
    std::string            key;
    const UiDiscoveryNode* before = nullptr;
    const UiDiscoveryNode* after = nullptr;
    bool                   sdkStringIdChanged = false;
    bool                   rawStringIdChanged = false;
    bool                   visibleChanged = false;
    bool                   flagsChanged = false;
    bool                   childCountChanged = false;
    bool                   rectChanged = false;
    bool                   areaChanged = false;
    bool                   elementTypeChanged = false;
    float                  areaDelta = 0.f;
};

struct UiPinnedCandidate {
    std::string rootLabel;
    std::string path;
    std::string note;
    std::string lastSdkStringId;
    std::string lastRawStringId;
};

struct UiPinnedResult {
    std::string rootLabel;
    std::string path;
    std::string note;
    std::string sdkStringId;
    std::string rawStringId;
    std::string parentSdkStringId;
    std::string parentRawStringId;
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
    bool        exists = false;
    bool        valid = false;
    bool        localVisible = false;
    bool        serviceVisible = false;
    bool        hasRect = false;
    std::string failureReason;
};

struct UiDiscoveryState {
    bool autoRefresh = false;
    bool requestedRefresh = false;
    bool showDiff = false;
    bool showPinnedCompare = true;
    bool includeInvisible = true;
    bool includeZeroRect = true;
    bool includeEmptyStringId = true;
    bool showTopLevelSummary = true;
    int  scanIntervalMs = 500;
    int  manualRootIndex = 2;
    char manualPath[128]{};
    char manualNote[128]{};

    std::vector<UiDiscoveryNode> liveNodes;
    std::vector<UiDiscoveryNode> baselineNodes;
    std::vector<UiDiscoveryNode> currentNodes;
    std::vector<UiDiscoveryDiffRow> diffRows;
    std::vector<UiPinnedCandidate> pins;
    std::vector<UiPinnedResult> pinnedBaseline;
    std::vector<UiPinnedResult> pinnedCurrent;
    size_t lastNodesVisited = 0;
    size_t lastVisibleRects = 0;
    bool   lastScanFailed = false;
    std::string lastError;
    std::chrono::steady_clock::time_point lastScanTime = std::chrono::steady_clock::time_point{};
    std::chrono::steady_clock::time_point baselineCaptureTime = std::chrono::steady_clock::time_point{};
    std::chrono::steady_clock::time_point currentCaptureTime = std::chrono::steady_clock::time_point{};
    std::chrono::steady_clock::time_point pinnedBaselineCaptureTime = std::chrono::steady_clock::time_point{};
    std::chrono::steady_clock::time_point pinnedCurrentCaptureTime = std::chrono::steady_clock::time_point{};
};

inline bool DiscoveryLooksHeapLike(uintptr_t address) {
    return address >= 0x10000ull && address <= 0x00007FFFFFFFFFFFull;
}

inline std::string DiscoveryAddressText(uintptr_t address) {
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%llX", static_cast<unsigned long long>(address));
    return buf;
}

inline std::string DiscoveryNarrowPrintable(std::wstring_view value) {
    std::string out;
    out.reserve(value.size());
    for (const wchar_t ch : value) {
        if (ch == 0) break;
        if (ch >= 32 && ch <= 126)
            out.push_back(static_cast<char>(ch));
        else if (ch == L'\t')
            out.push_back('\t');
        else if (ch == L'\n' || ch == L'\r')
            out.push_back(' ');
        else
            out.push_back('?');
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

inline std::string DiscoveryReadRawStringId(PluginSDK::Context* ctx, uintptr_t address) {
    if (!ctx || address == 0) return {};
    constexpr uintptr_t kRawStringIdOffset = 0x4c0;
    return DiscoveryNarrowPrintable(ctx->Memory.ReadStdWString(address + kRawStringIdOffset));
}

inline const char* DiscoveryPrimaryStringId(const UiDiscoveryNode& node) {
    if (!node.rawStringId.empty()) return node.rawStringId.c_str();
    if (!node.sdkStringId.empty()) return node.sdkStringId.c_str();
    return "(empty)";
}

inline const char* DiscoveryPrimaryStringId(const UiPinnedResult& result) {
    if (!result.rawStringId.empty()) return result.rawStringId.c_str();
    if (!result.sdkStringId.empty()) return result.sdkStringId.c_str();
    return "(empty)";
}

inline bool DiscoveryRectChanged(const UiDiscoveryNode& a, const UiDiscoveryNode& b) {
    if (a.hasRect != b.hasRect) return true;
    if (!a.hasRect && !b.hasRect) return false;
    return std::fabs(a.rectX - b.rectX) > 1.f || std::fabs(a.rectY - b.rectY) > 1.f
           || std::fabs(a.rectW - b.rectW) > 1.f || std::fabs(a.rectH - b.rectH) > 1.f;
}

inline uintptr_t DiscoveryWalkUpToRoot(PluginSDK::Context* ctx, uintptr_t start) {
    if (!ctx || !DiscoveryLooksHeapLike(start)) return 0;
    uintptr_t current = start;
    uintptr_t lastValid = start;
    for (int i = 0; i < 32; ++i) {
        const PluginSDK::UiElement element = ctx->Ui.Read(current);
        if (!element.Valid) break;
        lastValid = current;
        if (!DiscoveryLooksHeapLike(element.ParentAddress) || element.ParentAddress == current
            || element.ParentAddress == 0)
            break;
        current = element.ParentAddress;
    }
    return lastValid;
}

inline uintptr_t DiscoveryResolveRoot(PluginSDK::Context* ctx, std::string_view rootLabel) {
    if (!ctx) return 0;
    if (rootLabel == "GameUiRoot") return ctx->Ui.GetGameUiRoot();
    if (rootLabel == "UiRoot") return ctx->Ui.GetUiRoot();
    if (rootLabel == "TopUiRoot") return DiscoveryWalkUpToRoot(ctx, ctx->Ui.GetUiRoot());
    return 0;
}

inline std::vector<int> DiscoveryParsePathIndices(std::string_view path) {
    std::vector<int> indices;
    size_t start = 0;
    while (start < path.size()) {
        const size_t slash = path.find('/', start);
        const std::string_view token =
            slash == std::string_view::npos ? path.substr(start) : path.substr(start, slash - start);
        if (!token.empty() && token != "root") {
            bool ok = true;
            int value = 0;
            for (const char ch : token) {
                if (ch < '0' || ch > '9') {
                    ok = false;
                    break;
                }
                value = value * 10 + (ch - '0');
            }
            if (!ok) return {};
            indices.push_back(value);
        }
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    return indices;
}

inline std::string DiscoveryTrim(std::string_view value) {
    size_t start = 0;
    size_t end = value.size();
    while (start < end && (value[start] == ' ' || value[start] == '\t')) ++start;
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t')) --end;
    return std::string(value.substr(start, end - start));
}

inline std::string DiscoveryNormalizePinnedPath(const char* input, int* rootIndex = nullptr) {
    std::string text = input ? DiscoveryTrim(input) : std::string{};
    if (text.empty()) return {};

    if (const size_t pipe = text.find('|'); pipe != std::string::npos)
        text = DiscoveryTrim(text.substr(pipe + 1));

    for (int i = 0; i < 3; ++i) {
        const std::string prefix = std::string(kDiscoveryRootLabels[i]) + " ";
        if (text.rfind(prefix, 0) == 0) {
            if (rootIndex) *rootIndex = i;
            text = DiscoveryTrim(text.substr(prefix.size()));
            break;
        }
    }

    if (text == "root") return text;
    if (text.rfind("root/", 0) != 0) return {};
    return text;
}

inline std::string BuildDiscoveryCopyText(const UiDiscoveryNode& node) {
    std::ostringstream out;
    out << "Root: " << node.rootLabel << "\n";
    out << "Path: " << node.path << "\n";
    out << "ParentPath: " << node.parentPath << "\n";
    out << "SdkStringId: " << (node.sdkStringId.empty() ? "(empty)" : node.sdkStringId) << "\n";
    out << "RawStringId: " << (node.rawStringId.empty() ? "(empty)" : node.rawStringId) << "\n";
    out << "Address: " << DiscoveryAddressText(node.address) << "\n";
    out << "ParentAddress: " << DiscoveryAddressText(node.parentAddress) << "\n";
    out << "ParentSdkStringId: "
        << (node.parentSdkStringId.empty() ? "(empty)" : node.parentSdkStringId) << "\n";
    out << "ParentRawStringId: "
        << (node.parentRawStringId.empty() ? "(empty)" : node.parentRawStringId) << "\n";
    out << "Visible: " << (node.visible ? "true" : "false") << "\n";
    out << "HasRect: " << (node.hasRect ? "true" : "false") << "\n";
    out << "Rect: x=" << node.rectX << " y=" << node.rectY << " w=" << node.rectW << " h="
        << node.rectH << "\n";
    out << "Area: " << node.area << "\n";
    out << "ChildCount: " << node.childCount << "\n";
    out << "Depth: " << node.depth << "\n";
    out << "ElementType: 0x" << std::hex << node.elementType << std::dec << " ("
        << node.elementType << ")\n";
    out << "Flags: 0x" << std::hex << node.flags << std::dec << "\n";
    out << "ScaleIndex: " << static_cast<unsigned int>(node.scaleIndex) << "\n";
    return out.str();
}

inline std::string BuildPinnedCopyText(const UiPinnedResult& result) {
    std::ostringstream out;
    out << "Root: " << result.rootLabel << "\n";
    out << "Path: " << result.path << "\n";
    out << "Note: " << result.note << "\n";
    out << "Exists: " << (result.exists ? "true" : "false") << "\n";
    out << "Valid: " << (result.valid ? "true" : "false") << "\n";
    out << "LocalVisible: " << (result.localVisible ? "true" : "false") << "\n";
    out << "UiIsVisible: " << (result.serviceVisible ? "true" : "false") << "\n";
    out << "SdkStringId: " << (result.sdkStringId.empty() ? "(empty)" : result.sdkStringId) << "\n";
    out << "RawStringId: " << (result.rawStringId.empty() ? "(empty)" : result.rawStringId) << "\n";
    out << "ParentSdkStringId: "
        << (result.parentSdkStringId.empty() ? "(empty)" : result.parentSdkStringId) << "\n";
    out << "ParentRawStringId: "
        << (result.parentRawStringId.empty() ? "(empty)" : result.parentRawStringId) << "\n";
    out << "Address: " << DiscoveryAddressText(result.address) << "\n";
    out << "ParentAddress: " << DiscoveryAddressText(result.parentAddress) << "\n";
    out << "HasRect: " << (result.hasRect ? "true" : "false") << "\n";
    out << "Rect: x=" << result.rectX << " y=" << result.rectY << " w=" << result.rectW << " h="
        << result.rectH << "\n";
    out << "Area: " << result.area << "\n";
    out << "ChildCount: " << result.childCount << "\n";
    out << "ElementType: 0x" << std::hex << result.elementType << std::dec << " ("
        << result.elementType << ")\n";
    out << "Flags: 0x" << std::hex << result.flags << std::dec << "\n";
    out << "ScaleIndex: " << static_cast<unsigned int>(result.scaleIndex) << "\n";
    if (!result.failureReason.empty()) out << "FailureReason: " << result.failureReason << "\n";
    return out.str();
}

inline std::string BuildPinnedCompareCopyText(const UiPinnedCandidate& pin,
                                              const UiPinnedResult* before,
                                              const UiPinnedResult* after) {
    std::ostringstream out;
    out << "PinRoot: " << pin.rootLabel << "\n";
    out << "PinPath: " << pin.path << "\n";
    out << "PinNote: " << pin.note << "\n";
    out << "Baseline:\n";
    UiPinnedResult fallbackBefore;
    fallbackBefore.rootLabel = pin.rootLabel;
    fallbackBefore.path = pin.path;
    fallbackBefore.note = pin.note;
    out << BuildPinnedCopyText(before ? *before : fallbackBefore);
    out << "Current:\n";
    UiPinnedResult fallbackAfter;
    fallbackAfter.rootLabel = pin.rootLabel;
    fallbackAfter.path = pin.path;
    fallbackAfter.note = pin.note;
    out << BuildPinnedCopyText(after ? *after : fallbackAfter);
    return out.str();
}

inline std::string BuildDiscoveryDiffCopyText(const UiDiscoveryDiffRow& row) {
    auto writeNode = [](std::ostringstream& out, const char* prefix, const UiDiscoveryNode* node) {
        out << prefix << "Address: " << DiscoveryAddressText(node ? node->address : 0) << "\n";
        out << prefix << "SdkStringId: "
            << ((node && !node->sdkStringId.empty()) ? node->sdkStringId : std::string("(empty)"))
            << "\n";
        out << prefix << "RawStringId: "
            << ((node && !node->rawStringId.empty()) ? node->rawStringId : std::string("(empty)"))
            << "\n";
        out << prefix << "Visible: " << ((node && node->visible) ? "true" : "false") << "\n";
        out << prefix << "HasRect: " << ((node && node->hasRect) ? "true" : "false") << "\n";
        out << prefix << "Rect: x=" << (node ? node->rectX : 0.f) << " y=" << (node ? node->rectY : 0.f)
            << " w=" << (node ? node->rectW : 0.f) << " h=" << (node ? node->rectH : 0.f) << "\n";
        out << prefix << "Area: " << (node ? node->area : 0.f) << "\n";
        out << prefix << "ChildCount: " << (node ? node->childCount : 0) << "\n";
        out << prefix << "ElementType: 0x" << std::hex << (node ? node->elementType : 0) << std::dec
            << "\n";
        out << prefix << "Flags: 0x" << std::hex << (node ? node->flags : 0) << std::dec << "\n";
        out << prefix << "ParentPath: " << (node ? node->parentPath : std::string{}) << "\n";
        out << prefix << "ParentSdkStringId: "
            << ((node && !node->parentSdkStringId.empty()) ? node->parentSdkStringId
                                                           : std::string("(empty)"))
            << "\n";
        out << prefix << "ParentRawStringId: "
            << ((node && !node->parentRawStringId.empty()) ? node->parentRawStringId
                                                           : std::string("(empty)"))
            << "\n";
    };

    std::ostringstream out;
    out << "Key: " << row.key << "\n";
    out << "Kind: "
        << (row.kind == UiDiscoveryDiffKind::Changed
                ? "Changed"
                : row.kind == UiDiscoveryDiffKind::Appeared ? "Appeared" : "Disappeared")
        << "\n";
    if (row.before) {
        out << "BeforeRoot: " << row.before->rootLabel << "\n";
        out << "BeforePath: " << row.before->path << "\n";
    }
    if (row.after) {
        out << "AfterRoot: " << row.after->rootLabel << "\n";
        out << "AfterPath: " << row.after->path << "\n";
    }
    writeNode(out, "Old", row.before);
    writeNode(out, "New", row.after);
    return out.str();
}

inline void DrawDiscoveryHighlight(PluginSDK::Context* ctx, const UiDiscoveryNode& node) {
    if (!ctx || node.address == 0) return;
    float x = node.rectX;
    float y = node.rectY;
    float w = node.rectW;
    float h = node.rectH;
    if (!ctx->Ui.ComputeScreenRect(node.address, x, y, w, h) || w <= 0.f || h <= 0.f) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) return;
    const ImVec2 min(x, y);
    const ImVec2 max(x + w, y + h);
    dl->AddRect(min, max, IM_COL32(255, 200, 0, 255), 0.f, 0, 2.f);
    dl->AddRectFilled(min, max, IM_COL32(255, 200, 0, 24));
}

inline std::vector<UiDiscoveryNode> CaptureUiDiscoverySnapshot(UiDiscoveryState& state,
                                                               PluginSDK::Context* ctx) {
    state.lastNodesVisited = 0;
    state.lastVisibleRects = 0;
    state.lastScanFailed = false;
    state.lastError.clear();
    state.lastScanTime = std::chrono::steady_clock::now();

    std::vector<UiDiscoveryNode> nodes;
    if (!ctx) {
        state.lastScanFailed = true;
        state.lastError = "Context unavailable.";
        return nodes;
    }

    struct QueueItem {
        uintptr_t   address = 0;
        uintptr_t   parentAddress = 0;
        std::string rootLabel;
        std::string path;
        std::string parentPath;
        std::string parentSdkStringId;
        std::string parentRawStringId;
        int         depth = 0;
    };

    std::queue<QueueItem> queue;
    std::unordered_set<uintptr_t> visited;
    visited.reserve(4096);

    auto enqueueRoot = [&](uintptr_t root, const char* label) {
        if (root == 0) return;
        queue.push(QueueItem{root, 0, label, "root", {}, {}, {}, 0});
    };

    const uintptr_t gameUiRoot = ctx->Ui.GetGameUiRoot();
    const uintptr_t uiRoot = ctx->Ui.GetUiRoot();
    const uintptr_t topUiRoot = DiscoveryWalkUpToRoot(ctx, uiRoot);
    enqueueRoot(gameUiRoot, "GameUiRoot");
    if (uiRoot != 0 && uiRoot != gameUiRoot) enqueueRoot(uiRoot, "UiRoot");
    if (topUiRoot != 0 && topUiRoot != uiRoot && topUiRoot != gameUiRoot)
        enqueueRoot(topUiRoot, "TopUiRoot");

    if (queue.empty()) {
        state.lastScanFailed = true;
        state.lastError = "No UI root available.";
        return nodes;
    }

    constexpr int kMaxDepth = 24;
    constexpr size_t kMaxVisited = 30000;

    while (!queue.empty() && visited.size() < kMaxVisited) {
        QueueItem item = std::move(queue.front());
        queue.pop();
        if (item.address == 0 || item.depth > kMaxDepth) continue;
        if (!visited.insert(item.address).second) continue;
        state.lastNodesVisited = visited.size();

        const PluginSDK::UiElement element = ctx->Ui.Read(item.address);
        if (!element.Valid) continue;

        UiDiscoveryNode node;
        node.rootLabel = item.rootLabel;
        node.path = item.path;
        node.parentPath = item.parentPath;
        node.parentSdkStringId = item.parentSdkStringId;
        node.parentRawStringId = item.parentRawStringId;
        node.address = item.address;
        node.parentAddress = item.parentAddress;
        node.sdkStringId = ctx->Ui.GetStringId(item.address);
        node.rawStringId = DiscoveryReadRawStringId(ctx, item.address);
        node.childCount = element.ChildCount;
        node.depth = item.depth;
        node.elementType = element.ElementType;
        node.flags = element.Flags;
        node.scaleIndex = element.ScaleIndex;
        node.visible = element.IsVisible;

        float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
        if (ctx->Ui.ComputeScreenRect(item.address, x, y, w, h)) {
            node.hasRect = true;
            node.rectX = x;
            node.rectY = y;
            node.rectW = w;
            node.rectH = h;
            node.area = std::fabs(w * h);
            if (w > 0.f && h > 0.f) state.lastVisibleRects++;
        }

        nodes.push_back(node);

        const auto children = ctx->Ui.GetChildren(item.address);
        for (size_t i = 0; i < children.size(); ++i) {
            const uintptr_t child = children[i];
            if (child == 0) continue;
            std::string childPath = item.path;
            childPath.push_back('/');
            childPath += std::to_string(i);
            queue.push(QueueItem{child, item.address, item.rootLabel, std::move(childPath), item.path,
                                 node.sdkStringId, node.rawStringId, item.depth + 1});
        }
    }

    return nodes;
}

inline void RefreshUiDiscovery(UiDiscoveryState& state, PluginSDK::Context* ctx) {
    state.liveNodes = CaptureUiDiscoverySnapshot(state, ctx);
}

inline void RebuildUiDiscoveryDiff(UiDiscoveryState& state) {
    state.diffRows.clear();
    std::unordered_map<std::string, const UiDiscoveryNode*> beforeMap;
    std::unordered_map<std::string, const UiDiscoveryNode*> afterMap;
    beforeMap.reserve(state.baselineNodes.size());
    afterMap.reserve(state.currentNodes.size());

    auto keyFor = [](const UiDiscoveryNode& node) { return node.rootLabel + "|" + node.path; };
    for (const auto& node : state.baselineNodes) beforeMap.emplace(keyFor(node), &node);
    for (const auto& node : state.currentNodes) afterMap.emplace(keyFor(node), &node);

    for (const auto& [key, before] : beforeMap) {
        const auto it = afterMap.find(key);
        if (it == afterMap.end()) {
            state.diffRows.push_back(
                UiDiscoveryDiffRow{UiDiscoveryDiffKind::Disappeared, key, before, nullptr});
            continue;
        }

        const UiDiscoveryNode* after = it->second;
        UiDiscoveryDiffRow row;
        row.kind = UiDiscoveryDiffKind::Changed;
        row.key = key;
        row.before = before;
        row.after = after;
        row.sdkStringIdChanged = before->sdkStringId != after->sdkStringId;
        row.rawStringIdChanged = before->rawStringId != after->rawStringId;
        row.visibleChanged = before->visible != after->visible;
        row.flagsChanged = before->flags != after->flags;
        row.childCountChanged = before->childCount != after->childCount;
        row.rectChanged = DiscoveryRectChanged(*before, *after);
        row.areaChanged = std::fabs(before->area - after->area) > 1.f
                          || before->hasRect != after->hasRect;
        row.elementTypeChanged = before->elementType != after->elementType;
        row.areaDelta = std::fabs(before->area - after->area);
        if (row.sdkStringIdChanged || row.rawStringIdChanged || row.visibleChanged
            || row.flagsChanged || row.childCountChanged || row.rectChanged || row.areaChanged
            || row.elementTypeChanged)
            state.diffRows.push_back(std::move(row));
    }

    for (const auto& [key, after] : afterMap) {
        if (beforeMap.contains(key)) continue;
        state.diffRows.push_back(
            UiDiscoveryDiffRow{UiDiscoveryDiffKind::Appeared, key, nullptr, after});
    }

    std::sort(state.diffRows.begin(), state.diffRows.end(),
              [](const UiDiscoveryDiffRow& a, const UiDiscoveryDiffRow& b) {
                  auto priority = [](const UiDiscoveryDiffRow& row) {
                      if (row.kind == UiDiscoveryDiffKind::Changed) return 0;
                      if (row.kind == UiDiscoveryDiffKind::Appeared) return 1;
                      return 2;
                  };
                  const int pa = priority(a);
                  const int pb = priority(b);
                  if (pa != pb) return pa < pb;
                  if (a.kind == UiDiscoveryDiffKind::Changed && b.kind == UiDiscoveryDiffKind::Changed) {
                      const int sa = (a.rawStringIdChanged ? 64 : 0) + (a.visibleChanged ? 32 : 0)
                                     + (a.flagsChanged ? 16 : 0) + (a.childCountChanged ? 8 : 0)
                                     + (a.rectChanged ? 4 : 0) + (a.areaChanged ? 2 : 0)
                                     + (a.sdkStringIdChanged ? 1 : 0);
                      const int sb = (b.rawStringIdChanged ? 64 : 0) + (b.visibleChanged ? 32 : 0)
                                     + (b.flagsChanged ? 16 : 0) + (b.childCountChanged ? 8 : 0)
                                     + (b.rectChanged ? 4 : 0) + (b.areaChanged ? 2 : 0)
                                     + (b.sdkStringIdChanged ? 1 : 0);
                      if (sa != sb) return sa > sb;
                      if (a.areaDelta != b.areaDelta) return a.areaDelta > b.areaDelta;
                  }
                  const float aa = a.after ? a.after->area : (a.before ? a.before->area : 0.f);
                  const float ab = b.after ? b.after->area : (b.before ? b.before->area : 0.f);
                  if (aa != ab) return aa > ab;
                  return a.key < b.key;
              });
}

inline bool ShouldDisplayUiNode(const UiDiscoveryState& state, const UiDiscoveryNode& node) {
    if (!state.includeInvisible && !node.visible) return false;
    if (!state.includeZeroRect && (!node.hasRect || node.rectW <= 0.f || node.rectH <= 0.f))
        return false;
    if (!state.includeEmptyStringId && node.sdkStringId.empty() && node.rawStringId.empty())
        return false;
    return true;
}

inline bool DiscoveryPinExists(const UiDiscoveryState& state, std::string_view rootLabel,
                               std::string_view path) {
    for (const auto& pin : state.pins) {
        if (pin.rootLabel == rootLabel && pin.path == path) return true;
    }
    return false;
}

inline void DiscoveryAddPin(UiDiscoveryState& state, std::string rootLabel, std::string path,
                            std::string note = {}, std::string sdkStringId = {},
                            std::string rawStringId = {}) {
    int detectedRootIndex = -1;
    const std::string normalizedPath = DiscoveryNormalizePinnedPath(path.c_str(), &detectedRootIndex);
    if (detectedRootIndex >= 0) rootLabel = kDiscoveryRootLabels[detectedRootIndex];
    if (rootLabel.empty() || normalizedPath.empty()) return;
    if (DiscoveryPinExists(state, rootLabel, normalizedPath)) return;
    state.pins.push_back(UiPinnedCandidate{std::move(rootLabel), normalizedPath, std::move(note),
                                           std::move(sdkStringId), std::move(rawStringId)});
}

inline UiPinnedResult ResolvePinnedCandidate(PluginSDK::Context* ctx, const UiPinnedCandidate& pin) {
    UiPinnedResult result;
    result.rootLabel = pin.rootLabel;
    result.path = pin.path;
    result.note = pin.note;

    if (!ctx) {
        result.failureReason = "context unavailable";
        return result;
    }
    uintptr_t current = DiscoveryResolveRoot(ctx, pin.rootLabel);
    if (current == 0) {
        result.failureReason = "root unavailable";
        return result;
    }

    result.exists = true;
    const std::vector<int> indices = DiscoveryParsePathIndices(pin.path);
    if (pin.path.empty() || (indices.empty() && pin.path != "root")) {
        result.exists = false;
        result.failureReason = "invalid path format";
        return result;
    }

    for (size_t depth = 0; depth < indices.size(); ++depth) {
        const int index = indices[depth];
        const auto children = ctx->Ui.GetChildren(current);
        if (index < 0 || static_cast<size_t>(index) >= children.size()) {
            result.exists = false;
            result.failureReason = "child index out of range at step " + std::to_string(depth)
                                   + " (index " + std::to_string(index) + ")";
            return result;
        }
        result.parentAddress = current;
        current = children[static_cast<size_t>(index)];
        if (current == 0) {
            result.exists = false;
            result.failureReason = "failed at child index " + std::to_string(depth) + " (null child)";
            return result;
        }
    }

    result.address = current;
    const PluginSDK::UiElement element = ctx->Ui.Read(current);
    result.valid = element.Valid;
    if (!element.Valid) {
        result.failureReason = "Ui.Read invalid at resolved address";
        return result;
    }

    result.localVisible = element.IsVisible;
    result.serviceVisible = ctx->Ui.IsVisible(current);
    result.sdkStringId = ctx->Ui.GetStringId(current);
    result.rawStringId = DiscoveryReadRawStringId(ctx, current);
    result.childCount = element.ChildCount;
    result.elementType = element.ElementType;
    result.flags = element.Flags;
    result.scaleIndex = element.ScaleIndex;
    if (result.parentAddress != 0) {
        result.parentSdkStringId = ctx->Ui.GetStringId(result.parentAddress);
        result.parentRawStringId = DiscoveryReadRawStringId(ctx, result.parentAddress);
    }

    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
    if (ctx->Ui.ComputeScreenRect(current, x, y, w, h)) {
        result.hasRect = true;
        result.rectX = x;
        result.rectY = y;
        result.rectW = w;
        result.rectH = h;
        result.area = std::fabs(w * h);
    }
    return result;
}

inline std::vector<UiPinnedResult> CapturePinnedResults(PluginSDK::Context* ctx,
                                                        std::vector<UiPinnedCandidate>& pins) {
    std::vector<UiPinnedResult> results;
    results.reserve(pins.size());
    for (auto& pin : pins) {
        UiPinnedResult result = ResolvePinnedCandidate(ctx, pin);
        if (!result.sdkStringId.empty()) pin.lastSdkStringId = result.sdkStringId;
        if (!result.rawStringId.empty()) pin.lastRawStringId = result.rawStringId;
        results.push_back(std::move(result));
    }
    return results;
}

inline const UiPinnedResult* FindPinnedResult(const std::vector<UiPinnedResult>& results,
                                              const UiPinnedCandidate& pin) {
    for (const auto& result : results) {
        if (result.rootLabel == pin.rootLabel && result.path == pin.path) return &result;
    }
    return nullptr;
}

inline void DrawUiDiscoveryNodeRow(const UiDiscoveryNode& node, PluginSDK::Context* ctx,
                                   UiDiscoveryState* state = nullptr) {
    if (state) {
        ImGui::PushID(node.path.c_str());
        if (ImGui::SmallButton("Pin"))
            DiscoveryAddPin(*state, node.rootLabel, node.path, {}, node.sdkStringId, node.rawStringId);
        ImGui::SameLine();
        ImGui::PopID();
    }

    const std::string header = node.path + " | " + DiscoveryPrimaryStringId(node) + " | "
                               + DiscoveryAddressText(node.address);
    if (ImGui::Selectable(header.c_str(), false)) {
        const std::string copyText = BuildDiscoveryCopyText(node);
        ImGui::SetClipboardText(copyText.c_str());
    }
    if (ImGui::IsItemHovered()) DrawDiscoveryHighlight(ctx, node);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.72f, 0.72f, 1.f));
    ImGui::TextWrapped(
        "root=%s vis=%s rect=%s (%.0f, %.0f %.0fx%.0f) area=%.0f children=%d sdk=%s raw=%s parent=%s type=0x%04X flags=0x%08X scale=%u",
        node.rootLabel.c_str(), node.visible ? "yes" : "no", node.hasRect ? "yes" : "no",
        node.rectX, node.rectY, node.rectW, node.rectH, node.area, node.childCount,
        node.sdkStringId.empty() ? "(empty)" : node.sdkStringId.c_str(),
        node.rawStringId.empty() ? "(empty)" : node.rawStringId.c_str(),
        node.parentPath.empty() ? "(root)" : node.parentPath.c_str(), node.elementType, node.flags,
        static_cast<unsigned int>(node.scaleIndex));
    ImGui::PopStyleColor();
    ImGui::Separator();
}

inline void DrawUiDiscoveryTopLevelSummary(const UiDiscoveryState& state, PluginSDK::Context* ctx) {
    if (!state.showTopLevelSummary) return;
    if (!ImGui::TreeNodeEx("Top-Level Tree Summary", ImGuiTreeNodeFlags_DefaultOpen)) return;

    size_t shown = 0;
    for (const auto& node : state.liveNodes) {
        if (node.depth > 3) continue;
        if (!ShouldDisplayUiNode(state, node)) continue;
        ImGui::Indent(node.depth * 12.f);
        DrawUiDiscoveryNodeRow(node, ctx);
        ImGui::Unindent(node.depth * 12.f);
        if (++shown >= 60) break;
    }
    if (shown == 0) ImGui::TextDisabled("No top-level nodes match the current filters.");
    ImGui::TreePop();
}

inline void DrawUiDiscoveryDiffRows(UiDiscoveryState& state, PluginSDK::Context* ctx) {
    if (!state.showDiff) return;
    if (!ImGui::TreeNodeEx("Diff View", ImGuiTreeNodeFlags_DefaultOpen)) return;

    if (state.baselineNodes.empty() || state.currentNodes.empty()) {
        ImGui::TextDisabled("Capture both Baseline and Current snapshots first.");
        ImGui::TreePop();
        return;
    }

    ImGui::Text("Diff rows: %zu", state.diffRows.size());
    ImGui::BeginChild("##UiDiscoveryDiff", ImVec2(0.f, 320.f), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    const size_t limit = std::min<size_t>(state.diffRows.size(), 60);
    for (size_t i = 0; i < limit; ++i) {
        const auto& row = state.diffRows[i];
        const UiDiscoveryNode* focus = row.after ? row.after : row.before;
        std::string prefix = row.kind == UiDiscoveryDiffKind::Changed
                                 ? "[changed] "
                                 : row.kind == UiDiscoveryDiffKind::Appeared ? "[appeared] " : "[missing] ";
        ImGui::PushID(static_cast<int>(i));
        if (focus && ImGui::SmallButton("Pin"))
            DiscoveryAddPin(state, focus->rootLabel, focus->path, {}, focus->sdkStringId,
                            focus->rawStringId);
        ImGui::SameLine();
        const std::string header = prefix + row.key + " | "
                                   + (focus ? DiscoveryPrimaryStringId(*focus) : "(none)");
        if (ImGui::Selectable(header.c_str(), false)) {
            const std::string copyText = BuildDiscoveryDiffCopyText(row);
            ImGui::SetClipboardText(copyText.c_str());
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered() && focus) DrawDiscoveryHighlight(ctx, *focus);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.72f, 0.72f, 1.f));
        if (row.kind == UiDiscoveryDiffKind::Changed) {
            ImGui::TextWrapped(
                "sdk %s->%s | raw %s->%s | vis %s->%s | flags 0x%08X->0x%08X | children %d->%d | rect %s->%s | area %.0f->%.0f | type 0x%04X->0x%04X",
                row.before && !row.before->sdkStringId.empty() ? row.before->sdkStringId.c_str() : "(empty)",
                row.after && !row.after->sdkStringId.empty() ? row.after->sdkStringId.c_str() : "(empty)",
                row.before && !row.before->rawStringId.empty() ? row.before->rawStringId.c_str() : "(empty)",
                row.after && !row.after->rawStringId.empty() ? row.after->rawStringId.c_str() : "(empty)",
                row.before && row.before->visible ? "yes" : "no",
                row.after && row.after->visible ? "yes" : "no", row.before ? row.before->flags : 0,
                row.after ? row.after->flags : 0, row.before ? row.before->childCount : 0,
                row.after ? row.after->childCount : 0,
                row.before && row.before->hasRect ? "yes" : "no",
                row.after && row.after->hasRect ? "yes" : "no", row.before ? row.before->area : 0.f,
                row.after ? row.after->area : 0.f, row.before ? row.before->elementType : 0,
                row.after ? row.after->elementType : 0);
        } else if (row.kind == UiDiscoveryDiffKind::Appeared) {
            ImGui::TextWrapped(
                "new node: sdk=%s raw=%s vis=%s rect=%s area=%.0f children=%d flags=0x%08X type=0x%04X",
                row.after && !row.after->sdkStringId.empty() ? row.after->sdkStringId.c_str() : "(empty)",
                row.after && !row.after->rawStringId.empty() ? row.after->rawStringId.c_str() : "(empty)",
                row.after && row.after->visible ? "yes" : "no",
                row.after && row.after->hasRect ? "yes" : "no", row.after ? row.after->area : 0.f,
                row.after ? row.after->childCount : 0, row.after ? row.after->flags : 0,
                row.after ? row.after->elementType : 0);
        } else {
            ImGui::TextWrapped(
                "missing node: sdk=%s raw=%s vis=%s rect=%s area=%.0f children=%d flags=0x%08X type=0x%04X",
                row.before && !row.before->sdkStringId.empty() ? row.before->sdkStringId.c_str() : "(empty)",
                row.before && !row.before->rawStringId.empty() ? row.before->rawStringId.c_str() : "(empty)",
                row.before && row.before->visible ? "yes" : "no",
                row.before && row.before->hasRect ? "yes" : "no", row.before ? row.before->area : 0.f,
                row.before ? row.before->childCount : 0, row.before ? row.before->flags : 0,
                row.before ? row.before->elementType : 0);
        }
        ImGui::PopStyleColor();
        ImGui::Separator();
    }
    ImGui::EndChild();
    ImGui::TreePop();
}

inline void DrawUiDiscoveryLiveList(UiDiscoveryState& state, PluginSDK::Context* ctx) {
    if (!ImGui::TreeNodeEx("Current Nodes", ImGuiTreeNodeFlags_DefaultOpen)) return;
    ImGui::BeginChild("##UiDiscoveryCurrent", ImVec2(0.f, 320.f), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    size_t shown = 0;
    for (const auto& node : state.liveNodes) {
        if (!ShouldDisplayUiNode(state, node)) continue;
        DrawUiDiscoveryNodeRow(node, ctx, &state);
        if (++shown >= 40) break;
    }
    if (shown == 0) ImGui::TextDisabled("No current nodes match the filters.");
    ImGui::EndChild();
    ImGui::TreePop();
}

inline void DrawPinnedCandidatesSection(UiDiscoveryState& state, PluginSDK::Context* ctx) {
    if (!ImGui::TreeNodeEx("Pinned Candidates / Watch List", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::TextUnformatted("Add suspicious root/path candidates and compare only those paths.");
    ImGui::SetNextItemWidth(120.f);
    if (ImGui::BeginCombo("Root", kDiscoveryRootLabels[state.manualRootIndex])) {
        for (int i = 0; i < 3; ++i) {
            const bool selected = state.manualRootIndex == i;
            if (ImGui::Selectable(kDiscoveryRootLabels[i], selected)) state.manualRootIndex = i;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(220.f);
    ImGui::InputTextWithHint("Path", "root/1/22/0", state.manualPath, IM_ARRAYSIZE(state.manualPath));
    ImGui::SetNextItemWidth(220.f);
    ImGui::InputTextWithHint("Note", "Atlas candidate 1", state.manualNote,
                             IM_ARRAYSIZE(state.manualNote));
    if (ImGui::Button("Add Pin", ImVec2(90.f, 0.f))) {
        int normalizedRootIndex = state.manualRootIndex;
        const std::string normalizedPath =
            DiscoveryNormalizePinnedPath(state.manualPath, &normalizedRootIndex);
        if (!normalizedPath.empty()) {
            state.manualRootIndex = normalizedRootIndex;
            DiscoveryAddPin(state, kDiscoveryRootLabels[state.manualRootIndex], normalizedPath,
                            state.manualNote);
            std::snprintf(state.manualPath, IM_ARRAYSIZE(state.manualPath), "%s", normalizedPath.c_str());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Pins", ImVec2(90.f, 0.f))) {
        state.pins.clear();
        state.pinnedBaseline.clear();
        state.pinnedCurrent.clear();
    }

    ImGui::Spacing();
    if (ImGui::Button("Capture Pinned Baseline", ImVec2(170.f, 0.f))) {
        state.pinnedBaseline = CapturePinnedResults(ctx, state.pins);
        state.pinnedBaselineCaptureTime = std::chrono::steady_clock::now();
    }
    ImGui::SameLine();
    if (ImGui::Button("Capture Pinned Current", ImVec2(170.f, 0.f))) {
        state.pinnedCurrent = CapturePinnedResults(ctx, state.pins);
        state.pinnedCurrentCaptureTime = std::chrono::steady_clock::now();
    }
    ImGui::SameLine();
    if (ImGui::Button("Compare Pinned", ImVec2(120.f, 0.f))) state.showPinnedCompare = true;
    ImGui::SameLine();
    ImGui::Checkbox("Show pinned compare", &state.showPinnedCompare);
    ImGui::SameLine();
    if (ImGui::Button("Copy All Pinned Results", ImVec2(160.f, 0.f))) {
        std::ostringstream out;
        for (const auto& pin : state.pins) {
            out << "=== " << pin.rootLabel << " " << pin.path;
            if (!pin.note.empty()) out << " (" << pin.note << ")";
            out << " ===\n";
            out << BuildPinnedCompareCopyText(pin, FindPinnedResult(state.pinnedBaseline, pin),
                                              FindPinnedResult(state.pinnedCurrent, pin));
            out << "\n";
        }
        ImGui::SetClipboardText(out.str().c_str());
    }

    if (state.pins.empty()) {
        ImGui::TextDisabled("No pinned candidates yet.");
        ImGui::TreePop();
        return;
    }

    ImGui::Separator();
    for (size_t i = 0; i < state.pins.size(); ++i) {
        auto& pin = state.pins[i];
        const UiPinnedResult* before = FindPinnedResult(state.pinnedBaseline, pin);
        const UiPinnedResult* after = FindPinnedResult(state.pinnedCurrent, pin);
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton("Remove")) {
            state.pins.erase(state.pins.begin() + static_cast<std::ptrdiff_t>(i));
            ImGui::PopID();
            break;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy")) {
            const std::string copyText = BuildPinnedCompareCopyText(pin, before, after);
            ImGui::SetClipboardText(copyText.c_str());
        }
        ImGui::SameLine();
        ImGui::TextWrapped("%s %s%s%s", pin.rootLabel.c_str(), pin.path.c_str(),
                           pin.note.empty() ? "" : " | ", pin.note.empty() ? "" : pin.note.c_str());
        ImGui::TextDisabled("last sdk=%s | last raw=%s",
                            pin.lastSdkStringId.empty() ? "(empty)" : pin.lastSdkStringId.c_str(),
                            pin.lastRawStringId.empty() ? "(empty)" : pin.lastRawStringId.c_str());

        if (state.showPinnedCompare) {
            UiPinnedResult emptyBefore;
            emptyBefore.rootLabel = pin.rootLabel;
            emptyBefore.path = pin.path;
            emptyBefore.note = pin.note;
            UiPinnedResult emptyAfter;
            emptyAfter.rootLabel = pin.rootLabel;
            emptyAfter.path = pin.path;
            emptyAfter.note = pin.note;
            const UiPinnedResult& oldResult = before ? *before : emptyBefore;
            const UiPinnedResult& newResult = after ? *after : emptyAfter;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.72f, 0.72f, 1.f));
            ImGui::TextWrapped(
                "old exists=%s valid=%s vis(local/api)=%s/%s sdk=%s raw=%s flags=0x%08X children=%d rect=%s area=%.0f type=0x%04X",
                oldResult.exists ? "yes" : "no", oldResult.valid ? "yes" : "no",
                oldResult.localVisible ? "yes" : "no", oldResult.serviceVisible ? "yes" : "no",
                oldResult.sdkStringId.empty() ? "(empty)" : oldResult.sdkStringId.c_str(),
                oldResult.rawStringId.empty() ? "(empty)" : oldResult.rawStringId.c_str(),
                oldResult.flags, oldResult.childCount, oldResult.hasRect ? "yes" : "no",
                oldResult.area, oldResult.elementType);
            ImGui::TextWrapped(
                "new exists=%s valid=%s vis(local/api)=%s/%s sdk=%s raw=%s flags=0x%08X children=%d rect=%s area=%.0f type=0x%04X",
                newResult.exists ? "yes" : "no", newResult.valid ? "yes" : "no",
                newResult.localVisible ? "yes" : "no", newResult.serviceVisible ? "yes" : "no",
                newResult.sdkStringId.empty() ? "(empty)" : newResult.sdkStringId.c_str(),
                newResult.rawStringId.empty() ? "(empty)" : newResult.rawStringId.c_str(),
                newResult.flags, newResult.childCount, newResult.hasRect ? "yes" : "no",
                newResult.area, newResult.elementType);
            ImGui::TextWrapped(
                "old rect=(%.0f, %.0f %.0fx%.0f) parent sdk/raw=%s / %s | new rect=(%.0f, %.0f %.0fx%.0f) parent sdk/raw=%s / %s",
                oldResult.rectX, oldResult.rectY, oldResult.rectW, oldResult.rectH,
                oldResult.parentSdkStringId.empty() ? "(empty)" : oldResult.parentSdkStringId.c_str(),
                oldResult.parentRawStringId.empty() ? "(empty)" : oldResult.parentRawStringId.c_str(),
                newResult.rectX, newResult.rectY, newResult.rectW, newResult.rectH,
                newResult.parentSdkStringId.empty() ? "(empty)" : newResult.parentSdkStringId.c_str(),
                newResult.parentRawStringId.empty() ? "(empty)" : newResult.parentRawStringId.c_str());
            if (!oldResult.failureReason.empty() || !newResult.failureReason.empty()) {
                ImGui::TextWrapped("old fail=%s | new fail=%s",
                                   oldResult.failureReason.empty() ? "(none)" : oldResult.failureReason.c_str(),
                                   newResult.failureReason.empty() ? "(none)" : newResult.failureReason.c_str());
            }
            ImGui::PopStyleColor();
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::TreePop();
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

    ImGui::Spacing();
    if (ImGui::Button("Capture Baseline / Closed UI", ImVec2(190.f, 0.f))) {
        state.baselineNodes = CaptureUiDiscoverySnapshot(state, ctx);
        state.baselineCaptureTime = state.lastScanTime;
        if (!state.currentNodes.empty()) RebuildUiDiscoveryDiff(state);
    }
    ImGui::SameLine();
    if (ImGui::Button("Capture Current / Open UI", ImVec2(190.f, 0.f))) {
        state.currentNodes = CaptureUiDiscoverySnapshot(state, ctx);
        state.currentCaptureTime = state.lastScanTime;
        if (!state.baselineNodes.empty()) RebuildUiDiscoveryDiff(state);
    }
    ImGui::SameLine();
    if (ImGui::Button("Show Diff", ImVec2(90.f, 0.f))) {
        state.showDiff = true;
        if (!state.baselineNodes.empty() && !state.currentNodes.empty()) RebuildUiDiscoveryDiff(state);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Diff enabled", &state.showDiff);

    ImGui::Spacing();
    ImGui::Checkbox("Include invisible nodes", &state.includeInvisible);
    ImGui::SameLine();
    ImGui::Checkbox("Include zero/odd rect nodes", &state.includeZeroRect);
    ImGui::SameLine();
    ImGui::Checkbox("Include empty StringId nodes", &state.includeEmptyStringId);
    ImGui::SameLine();
    ImGui::Checkbox("Show top-level summary", &state.showTopLevelSummary);
    ImGui::TextDisabled("SDK StringId may be empty/stale; raw +0x4c0 StringId is shown for discovery.");

    if (state.lastScanTime.time_since_epoch().count() != 0) {
        const auto ageMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - state.lastScanTime).count();
        ImGui::Text("Last scan age: %lld ms", static_cast<long long>(ageMs));
    } else {
        ImGui::TextDisabled("No UI discovery scan yet.");
    }
    ImGui::Text("Visited nodes: %zu", state.lastNodesVisited);
    ImGui::Text("Rects resolved: %zu", state.lastVisibleRects);
    ImGui::Text("Live nodes stored: %zu", state.liveNodes.size());
    ImGui::Text("Baseline nodes: %zu | Current nodes: %zu | Diff rows: %zu",
                state.baselineNodes.size(), state.currentNodes.size(), state.diffRows.size());
    if (state.lastScanFailed) {
        ImGui::TextColored(ImVec4(1.f, 0.45f, 0.45f, 1.f), "Scan failed: %s",
                           state.lastError.c_str());
    }

    DrawPinnedCandidatesSection(state, ctx);
    DrawUiDiscoveryDiffRows(state, ctx);
    DrawUiDiscoveryTopLevelSummary(state, ctx);
    DrawUiDiscoveryLiveList(state, ctx);

    ImGui::Unindent(12.f);
}

} // namespace RadarUi
