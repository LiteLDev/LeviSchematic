# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.2.0] - 2026-11-22

### Fixed

- Material list stayed empty for disabled placements, including schematics opened from the load screen's material list button.
- Material list kept showing the area analysis result when opened afterwards from the load screen or placement configuration.

### Changed

- Material list no longer loads schematics: unloaded files are read in the background and counted as all missing (once, cached); placements are counted against the world, showing progress only for the first count and updating silently afterwards.
- Material list counts liquids as buckets (water, lava, bubble column, powder snow) including second-layer (waterlogging) water, and skips technical blocks, as in JE.
- Unloading a schematic no longer asks for confirmation.
- Rebuild the menu UI after the Litematica (JE) screens: breadcrumb titles, placement list / configuration, loaded schematics, load browser with info panel, schematic manager, area selection browser, area editor, sortable material list and tabbed configuration. Buttons show hover descriptions; +/- buttons and cycle buttons react to right click and Shift/Alt/Ctrl.

### Add

- Schematic manager: rename schematic / change author (litematic), export mcstructure ⇄ litematic, rename / copy / delete files.
- Hotkey advanced settings (activate on, context, allow empty / extra keys, order sensitive, exclusive, cancel further processing), opened from the settings icon next to each binding.
- Item icons in the material list.
- Area selection files (`area_selections/*.json`), placement enclosing box, load into memory without a placement, material list ignore / multiplier / clear cache / TXT-CSV-JSON export and area analysis.

## [1.1.0] - 2026-11-21

### Add

- Add `.litematic` file support, (only block, but some block status maybe error) [#10]

### Fixed

- Fix waterlogged block don't render water error

## [1.0.3] - 2026-11-20

### Fixed

- Loading a schematic while joined to a server failed with "World registry is unavailable" [#11]. Palette resolution, content reload, session restore and synchronous selection export now use the client Level instead of the in-process server Level.

- Build error

## [1.0.2] - 2026-11-20

### Fixed

- Loading a schematic while joined to a server failed with "World registry is unavailable" [#11]. Palette resolution, content reload, session restore and synchronous selection export now use the client Level instead of the in-process server Level.

[#10]: https://github.com/LiteLDev/LeviSchematic/issues/10
[#11]: https://github.com/LiteLDev/LeviSchematic/issues/11

[Unreleased]: https://github.com/LiteLDev/LeviSchematic/compare/v1.2.0...HEAD
[1.2.0]: https://github.com/LiteLDev/LeviSchematic/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/LiteLDev/LeviSchematic/compare/v1.0.3...v1.1.0
[1.0.3]: https://github.com/LiteLDev/LeviSchematic/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/LiteLDev/LeviSchematic/compare/v1.0.1...v1.0.2
