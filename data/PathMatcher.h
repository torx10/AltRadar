#pragma once

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace RadarData {

inline std::string NormalizeAreaKey(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

inline bool AreaKeysEqual(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i]))
            != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

inline std::string NormalizePathLower(std::string_view path) {
    std::string out(path);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

inline std::string NarrowPath(std::wstring_view path) {
    std::string out;
    out.reserve(path.size());
    for (wchar_t c : path)
        out.push_back(static_cast<char>(c));
    return out;
}

// Suffix after ':' (e.g. "1-y:1", "[]"). Empty when path has no suffix.
inline std::string ExtractTileSuffix(std::string_view path) {
    const auto colon = path.find(':');
    if (colon == std::string::npos) return {};
    return NormalizePathLower(path.substr(colon + 1));
}

inline bool TileSuffixMatches(std::string_view patternSuffix, std::string_view candidateSuffix) {
    if (patternSuffix.empty() || patternSuffix == "[]") return true;
    return patternSuffix == candidateSuffix;
}

// Game updates may use .tdtx, .tdbx, .tdb, .ot — match on path stem only.
inline std::string CanonicalTerrainPath(std::string_view path) {
    std::string s = NormalizePathLower(path);
    const auto colon = s.find(':');
    if (colon != std::string::npos) s.resize(colon);
    static const char* kExts[] = {".tdtx", ".tdbx", ".tdb", ".ot", ".idle"};
    for (const char* ext : kExts) {
        const size_t elen = strlen(ext);
        if (s.size() >= elen && s.compare(s.size() - elen, elen, ext) == 0) {
            s.resize(s.size() - elen);
            break;
        }
    }
    return s;
}

struct CompiledPattern {
    std::string normalized;
    std::string tileSuffix;
    bool        hasWildcard = false;
};

struct CompiledCandidate {
    std::string normalized;
    std::string tileSuffix;
};

inline CompiledPattern CompilePattern(std::string_view path) {
    CompiledPattern p;
    p.tileSuffix = ExtractTileSuffix(path);
    p.normalized = CanonicalTerrainPath(path);
    p.hasWildcard = p.normalized.find('*') != std::string::npos;
    return p;
}

inline CompiledCandidate CompileCandidate(std::string_view path) {
    CompiledCandidate c;
    c.tileSuffix = ExtractTileSuffix(path);
    c.normalized = CanonicalTerrainPath(path);
    return c;
}

inline CompiledCandidate CompileCandidate(std::wstring_view path) {
    return CompileCandidate(NarrowPath(path));
}

inline bool MatchPattern(const CompiledPattern& pattern, const CompiledCandidate& candidate) {
    if (pattern.normalized.empty()) return false;
    if (!TileSuffixMatches(pattern.tileSuffix, candidate.tileSuffix)) return false;
    const std::string_view cand = candidate.normalized;
    if (!pattern.hasWildcard) {
        return cand == pattern.normalized;
    }
    const auto& pat = pattern.normalized;
    const size_t onlyStar = pat.find('*');
    // Trailing * only: match entity paths like .../AldurRuneEncounterController
    if (onlyStar != std::string::npos && onlyStar == pat.size() - 1
        && pat.find('*', onlyStar + 1) == std::string::npos) {
        const std::string_view prefix = std::string_view(pat).substr(0, onlyStar);
        if (prefix.empty()) return true;
        if (cand.size() >= prefix.size()
            && cand.compare(cand.size() - prefix.size(), prefix.size(), prefix) == 0)
            return true;
        return cand.size() >= prefix.size()
               && cand.compare(0, prefix.size(), prefix) == 0;
    }
    size_t pi = 0, ci = 0;
    while (pi < pat.size() && ci < cand.size()) {
        if (pat[pi] == '*') {
            ++pi;
            if (pi >= pat.size()) return true;
            const size_t next = cand.find(pat[pi], ci);
            if (next == std::string::npos) return false;
            ci = next;
            continue;
        }
        if (pat[pi] != cand[ci]) return false;
        ++pi;
        ++ci;
    }
    while (pi < pat.size() && pat[pi] == '*') ++pi;
    return pi >= pat.size() && ci >= cand.size();
}

inline bool MatchPattern(const CompiledPattern& pattern, std::string_view candidateRaw) {
    return MatchPattern(pattern, CompileCandidate(candidateRaw));
}

struct PatternMatcherSet {
    std::vector<CompiledPattern> patterns;
    void Add(std::string_view path) { patterns.push_back(CompilePattern(path)); }
    bool MatchesAny(std::string_view candidate) const {
        for (const auto& p : patterns)
            if (MatchPattern(p, candidate)) return true;
        return false;
    }
};

} // namespace RadarData
