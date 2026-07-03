# Alt Radar — POE2Fixer Radar Fork

Self-contained radar overlay for Path of Exile 2: walkable terrain map, entity icons, and POI labels. This fork stays distinct from the official POE2Fixer Radar release. **No pathfinder / path lines.**

## Performance

- Overlay draws from **baked caches** rebuilt on area change only (not per frame).
- Walkable mesh is decimated (default 4×) and pre-projected to screen space.
- POI targets use `EnumerateTgtLocations` once per area, not every frame.
- `SetIncludeSleepingEntities` is enabled **only** during Add POI / Add Entity map picker.

## Install

1. Build `AltRadar.sln` Release|x64 → `bin/Release/AltRadar.dll`
2. Copy into POE2Fixer:

```
POE2Fixer/Plugins/AltRadar/
  AltRadar.dll
```

3. Enable **Alt Radar** in the Plugins tab.

On first run, Alt Radar creates `logs/`, `config/`, `config/settings.json`,
`config/display_rules.json`, and `config/targets/`. Optional target JSON files in
`config/targets/` add curated landmarks; missing files are skipped.

## Settings UI

Open the Plugins tab → **Alt Radar** → three tabs:

- **General Settings** — overlay toggle, walkable map, visibility rules
- **Display Rules** — generated vector marker rules and sizes
- **Objects** — POI targets by area, Add POI/Entity from map

## SDK

Targets Plugin SDK **v6**. Bundled headers in `sdk/` must match the host.

## Build

```text
MSBuild AltRadar.sln -p:Configuration=Release -p:Platform=x64
```

Requires Visual Studio 2022 Build Tools (v143), Windows SDK, D3D11.
