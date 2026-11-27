# Alt Radar — POE2Fixer Radar Fork

Self-contained radar overlay for Path of Exile 2: walkable terrain map, entity icons, and POI labels. This fork stays distinct from the official POE2Fixer Radar release. **No pathfinder / path lines.**

Current version: **AltRadar v0.1.0**

## Performance

- Overlay draws from **baked caches** rebuilt on area change only (not per frame).
- Walkable mesh is decimated (default 4×) and pre-projected to screen space.
- POI targets use `EnumerateTgtLocations` once per area, not every frame.
- `SetIncludeSleepingEntities` is enabled **only** during Add POI / Add Entity map picker.

## Install

1. Build `AltRadar.sln` (repo root) Release|x64 → `Plugins/AltRadar/bin/Release/AltRadar.dll`
2. Copy into POE2Fixer:

```
POE2Fixer/Plugins/AltRadar/
  AltRadar.dll
```

3. Enable **Alt Radar** in the Plugins tab.

On first run, Alt Radar creates `logs/`, `config/`, `config/settings.json`,
`config/display_rules.json`, and `config/targets/`.

`config/display_rules.json`, `config/targets/acts.json`,
`config/targets/endgame.json`, and `config/targets/ignore.json` are embedded in
`AltRadar.dll` and generated on first run if missing.

- Existing target JSON files are never overwritten.
- Existing `config/display_rules.json` is never overwritten.
- You may edit generated target JSON files directly.
- Delete generated `display_rules.json` or target JSON to regenerate the default
  copy on next start.
- If a generated file is corrupt, Alt Radar logs a warning and uses embedded
  defaults in memory without overwriting the corrupt file.
- `config/targets/user.json` stays separate and is only created when user edits
  exist.

Minimum package remains:

```text
POE2Fixer/Plugins/AltRadar/
  AltRadar.dll
```

## Settings UI

Open the Plugins tab → **Alt Radar** → three tabs:

- **General Settings** — overlay toggle, walkable map, visibility rules
- **Display Rules** — generated vector marker rules and sizes; rules can also match an entity's
  model (`Model`), which is how the Vaal, Ezomyte, Ventor's and Etched Beetle strongboxes are told apart
- **Objects** — landmarks by area, Add POI/Entity from map
- **Room Targets** (Objects tab, Add Room from Map) — a marker at the centre of every room whose
  name matches a pattern, for one area or every area of its category (Acts or Endgame); saved in
  `config/targets/rooms.json`

## SDK

Targets Plugin SDK **v6**. Headers come from the shared `POEFixer/plugin_sdk/` and `POEFixer/imgui/`
folders at the repo root (kept identical to upstream POEFixer/Radar - do not edit them)
and must match the host.

## Build

```text
MSBuild AltRadar.sln -p:Configuration=Release -p:Platform=x64
```

Requires Visual Studio 2022 Build Tools (v143), Windows SDK, D3D11.
