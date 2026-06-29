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
  assets/icons.png
  config/settings.json
  config/icons.json
  config/targets/acts.json
  config/targets/endgame.json
  config/targets/ignore.json
```

3. Enable **Alt Radar** in the Plugins tab.

If `assets/icons.png` is missing from the repo, run:

```powershell
.\scripts\copy-radar-assets.ps1
```

## Settings UI

Open the Plugins tab → **Alt Radar** → three tabs:

- **General Settings** — overlay toggle, walkable map, visibility rules
- **Icon Management** — per-category icons and sizes
- **Objects** — POI targets by area, Add POI/Entity from map

## First-run migration

If plugin `config/` is empty, the plugin may import once from legacy host paths next to Fixer.exe (`Configs/radar/`, `Resources/radar/`), then never read them again.

## SDK

Targets Plugin SDK **v6**. Bundled headers in `sdk/` must match the host.

## Build

```text
MSBuild AltRadar.sln -p:Configuration=Release -p:Platform=x64
```

Requires Visual Studio 2022 Build Tools (v143), Windows SDK, D3D11.
