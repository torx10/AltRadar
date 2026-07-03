param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectDir
)

$ErrorActionPreference = 'Stop'

$projectPath = [System.IO.Path]::GetFullPath($ProjectDir)
$targetsDir = Join-Path $projectPath 'config\targets'
$outputPath = Join-Path $projectPath 'data\EmbeddedTargets.h'

$sources = @(
    @{ Name = 'Acts'; File = 'acts.json' },
    @{ Name = 'Endgame'; File = 'endgame.json' },
    @{ Name = 'Ignore'; File = 'ignore.json' }
)

$chunkSize = 120

function Get-Sha256Hex([byte[]]$bytes) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return [System.BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-', '').ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

foreach ($source in $sources) {
    $path = Join-Path $targetsDir $source.File
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing target default source: $path"
    }
}

$builder = New-Object System.Text.StringBuilder
[void]$builder.AppendLine('#pragma once')
[void]$builder.AppendLine()
[void]$builder.AppendLine('namespace RadarData::EmbeddedTargets {')
[void]$builder.AppendLine()

foreach ($source in $sources) {
    $path = Join-Path $targetsDir $source.File
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $content = [System.Text.Encoding]::UTF8.GetString($bytes)
    $parsed = $content | ConvertFrom-Json
    $base64 = [System.Convert]::ToBase64String($bytes)
    [void]$builder.AppendLine("inline constexpr const char* k$($source.Name)JsonBase64 =")
    $reconstructedBase64 = New-Object System.Text.StringBuilder
    for ($offset = 0; $offset -lt $base64.Length; $offset += $chunkSize) {
        $length = [Math]::Min($chunkSize, $base64.Length - $offset)
        $chunk = $base64.Substring($offset, $length)
        [void]$reconstructedBase64.Append($chunk)
        [void]$builder.AppendLine('    "' + $chunk + '"')
    }
    [void]$builder.AppendLine(';')
    [void]$builder.AppendLine()

    $reconstructedBytes = [System.Convert]::FromBase64String($reconstructedBase64.ToString())
    $sourceHash = Get-Sha256Hex $bytes
    $reconstructedHash = Get-Sha256Hex $reconstructedBytes
    $match = $sourceHash -eq $reconstructedHash
    if (-not $match) {
        throw "Embedded payload mismatch after Base64 reconstruction: $($source.File)"
    }
    $properties = @($parsed.PSObject.Properties)
    $groups = $properties.Count
    $items = 0
    foreach ($property in $properties) {
        if ($property.Value -is [System.Array]) { $items += $property.Value.Count }
    }
    Write-Host "$($source.File): source sha256=$sourceHash embedded sha256=$reconstructedHash match=yes groups=$groups items=$items"
}

[void]$builder.AppendLine('} // namespace RadarData::EmbeddedTargets')

[System.IO.File]::WriteAllText($outputPath, $builder.ToString(), [System.Text.Encoding]::UTF8)
