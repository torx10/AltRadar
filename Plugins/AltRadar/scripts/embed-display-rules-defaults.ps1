param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectDir
)

$ErrorActionPreference = 'Stop'

$projectPath = [System.IO.Path]::GetFullPath($ProjectDir)
$sourcePath = Join-Path $projectPath 'config\display_rules.json'
$outputPath = Join-Path $projectPath 'data\DefaultDisplayRules.h'
$chunkSize = 120

function Get-Sha256Hex([byte[]]$bytes) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return [System.BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-', '').ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing display rule default source: $sourcePath"
}

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$bytes = [System.IO.File]::ReadAllBytes($sourcePath)
$content = [System.Text.Encoding]::UTF8.GetString($bytes)
if ($content.Length -gt 0 -and $content[0] -eq [char]0xfeff) {
    $content = $content.Substring(1)
}
$parsed = $content | ConvertFrom-Json
if ($null -eq $parsed.SchemaVersion) {
    throw 'display_rules.json missing SchemaVersion'
}
if ($null -eq $parsed.DisplayRules -or -not ($parsed.DisplayRules -is [System.Array])) {
    throw 'display_rules.json missing DisplayRules array'
}

$ids = New-Object 'System.Collections.Generic.HashSet[string]'
$duplicateIds = 0
foreach ($rule in $parsed.DisplayRules) {
    if ([string]::IsNullOrWhiteSpace([string]$rule.Id)) {
        throw "display_rules.json contains empty Id for rule: $($rule.Name)"
    }
    if (-not $ids.Add([string]$rule.Id)) {
        $duplicateIds++
    }
}
if ($duplicateIds -ne 0) {
    throw "display_rules.json contains duplicate Id count: $duplicateIds"
}

$base64 = [System.Convert]::ToBase64String($bytes)
$builder = New-Object System.Text.StringBuilder
[void]$builder.AppendLine('#pragma once')
[void]$builder.AppendLine()
[void]$builder.AppendLine('#include <algorithm>')
[void]$builder.AppendLine('#include <string>')
[void]$builder.AppendLine('#include <string_view>')
[void]$builder.AppendLine()
[void]$builder.AppendLine('namespace RadarData::DefaultDisplayRules {')
[void]$builder.AppendLine()
[void]$builder.AppendLine('inline constexpr const char* kJsonBase64 =')

$reconstructedBase64 = New-Object System.Text.StringBuilder
for ($offset = 0; $offset -lt $base64.Length; $offset += $chunkSize) {
    $length = [Math]::Min($chunkSize, $base64.Length - $offset)
    $chunk = $base64.Substring($offset, $length)
    [void]$reconstructedBase64.Append($chunk)
    [void]$builder.AppendLine('    "' + $chunk + '"')
}
[void]$builder.AppendLine(';')
[void]$builder.AppendLine()
[void]$builder.AppendLine('inline bool DecodeJson(std::string& out) {')
[void]$builder.AppendLine('    static constexpr unsigned char invalid = 255;')
[void]$builder.AppendLine('    unsigned char table[256];')
[void]$builder.AppendLine('    std::fill(table, table + 256, invalid);')
[void]$builder.AppendLine('    for (unsigned char c = ''A''; c <= ''Z''; ++c) table[c] = c - ''A'';')
[void]$builder.AppendLine('    for (unsigned char c = ''a''; c <= ''z''; ++c) table[c] = c - ''a'' + 26;')
[void]$builder.AppendLine('    for (unsigned char c = ''0''; c <= ''9''; ++c) table[c] = c - ''0'' + 52;')
[void]$builder.AppendLine('    table[static_cast<unsigned char>(''+'')] = 62;')
[void]$builder.AppendLine('    table[static_cast<unsigned char>(''/'')] = 63;')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('    out.clear();')
[void]$builder.AppendLine('    out.reserve(std::char_traits<char>::length(kJsonBase64) * 3 / 4);')
[void]$builder.AppendLine('    int val = 0;')
[void]$builder.AppendLine('    int bits = -8;')
[void]$builder.AppendLine('    for (unsigned char c : std::string_view(kJsonBase64)) {')
[void]$builder.AppendLine('        if (c == ''='') break;')
[void]$builder.AppendLine('        const unsigned char decoded = table[c];')
[void]$builder.AppendLine('        if (decoded == invalid) return false;')
[void]$builder.AppendLine('        val = (val << 6) | decoded;')
[void]$builder.AppendLine('        bits += 6;')
[void]$builder.AppendLine('        if (bits >= 0) {')
[void]$builder.AppendLine('            out.push_back(static_cast<char>((val >> bits) & 0xff));')
[void]$builder.AppendLine('            bits -= 8;')
[void]$builder.AppendLine('        }')
[void]$builder.AppendLine('    }')
[void]$builder.AppendLine('    if (out.size() >= 3 && static_cast<unsigned char>(out[0]) == 0xEF &&')
[void]$builder.AppendLine('        static_cast<unsigned char>(out[1]) == 0xBB &&')
[void]$builder.AppendLine('        static_cast<unsigned char>(out[2]) == 0xBF) out.erase(0, 3);')
[void]$builder.AppendLine('    return true;')
[void]$builder.AppendLine('}')
[void]$builder.AppendLine()
[void]$builder.AppendLine('} // namespace RadarData::DefaultDisplayRules')

$reconstructedBytes = [System.Convert]::FromBase64String($reconstructedBase64.ToString())
$sourceHash = Get-Sha256Hex $bytes
$reconstructedHash = Get-Sha256Hex $reconstructedBytes
if ($sourceHash -ne $reconstructedHash) {
    throw 'Embedded display_rules payload mismatch after Base64 reconstruction'
}

[System.IO.File]::WriteAllText($outputPath, $builder.ToString(), $utf8NoBom)
Write-Host "display_rules.json: source sha256=$sourceHash embedded sha256=$reconstructedHash match=yes rules=$($parsed.DisplayRules.Count) duplicates=$duplicateIds"
