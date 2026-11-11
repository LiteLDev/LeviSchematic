# LeviSchematic

[中文说明](README_zh.md)

## Introduction

LeviSchematic is a Bedrock client-side schematic projection mod built around `.mcstructure` files.
It lets players load saved structures into the world as translucent projections, manage multiple placements, adjust transforms, mark selections, export in-world regions back to `.mcstructure`, and compare projected blocks with the actual world state.

## Features

### Structure Loading And Placement Management

- Loads `.mcstructure` files through `/schem load`
- Lists available schematic files through `/schem files`
- Supports multiple loaded placements at the same time
- Lets you select, remove, disable, hide, inspect, or clear placements

### Placement Editing

- Moves the selected placement by relative offsets
- Sets a new placement origin directly
- Rotates the selected placement by `cw90`, `ccw90`, or `r180`
- Mirrors the selected placement on the `x` or `z` axis, or clears mirroring
- Resets transforms back to the original state
- Supports per-block projection edits such as hide, set, and clear override

### Projection Rendering And Verification

- Renders projected blocks as translucent ghost blocks in the world
- Block-entity models share the projection composite with terrain. `projection.alpha` is the background weight (0 opaque, 1 invisible); apply changes with `/schem config reload`.
- Refreshes projection chunks after placement edits
- Supports a global visible Y range filter for projection display
- Tracks expected blocks from loaded placements
- Compares projected blocks with actual world blocks
- Matched blocks are hidden; missing/unknown blocks use the normal projection; type/property errors use separate Hull batches
- Applies Y filtering to projected blocks, block entities and Hull
- Shows a native-font information panel while holding a stick in the main hand, with matte green text on a translucent black background
- Displays the mode, selected schematic filename and selection corners or placement origin, size and transforms. Use Settings → Info overlays → HUD layout, or `/schem ui hud`, to drag each of the four mod HUD panels and set its size to 50%–200% with the slider or numeric input. Leaving saves automatically; individual and global resets are available.
- The editor shows examples for inactive panels. Normal gameplay retains each panel's visibility conditions. `hud.panels` stores normalized safe-area positions and `scalePercent` for projection, target, materials and verifier HUDs; existing `hud.position` supplies the legacy default side.
- Pick-block and easy-place operation feedback is shown only with `diagnostics=true`.
- Hides the panel in menus and when the game HUD is hidden; long filenames are shortened to fit

### Selection And Export

- Stores two selection corners with `pos1` and `pos2`
- Shows a wireframe box for completed selections
- Saves the selected region as a `.mcstructure` file with `/schem save`
- Supports a selection mode that can be toggled in-game

## Command Usage

All commands use the `/schem` prefix.

### General

| Command | Purpose | Usage Notes |
|------|------|------|
| `/schem` | Shows the built-in command help summary. | Use it when you need a quick reminder of the available subcommands. |
| `/schem files` | Lists all `.mcstructure` files found in the schematic directory. | Use this before loading if you are not sure about the filename. |

### Loading

| Command | Purpose | Usage Notes |
|------|------|------|
| `/schem load <filename>` | Loads a schematic at the player's current position. | `filename` can be given with or without the `.mcstructure` extension. |
| `/schem load <filename> <x> <y> <z>` | Loads a schematic at the specified world position. | Returns a queued task ID immediately; completion reports the new placement ID on a client tick. |

Examples:

```text
/schem load house
/schem load tower 100 64 -32
```

Loads use one background file/NBT worker (four unfinished requests by default), then budgeted foreground preparation. A completed load means the placement has been published; verification, fake-world preparation and geometry become available progressively. Cancelling a task does not remove an already published placement.

| Command | Purpose |
|---|---|
| `/schem load status <taskId>` | Shows phase, processed block indices and completion/error details. |
| `/schem load cancel <taskId>` | Cancels an unfinished load in the current session. |
| `/schem config reload` | Validates `config.json` and queues application on the next client tick. |
| `/schem config save` | Saves the most recent valid submitted/applied configuration. |

### Placement Management

| Command | Purpose | Usage Notes |
|------|------|------|
| `/schem list` | Lists all loaded placements. | Shows ID, name, origin, transform, enabled state, and render state. |
| `/schem select <id>` | Selects a placement as the current active placement. | Most transform and block edit commands work on the selected placement. |
| `/schem remove` | Removes the currently selected placement. | Use after selecting a placement with `/schem select`. |
| `/schem remove <id>` | Removes the specified placement by ID. | Useful when you know the target placement ID directly. |
| `/schem toggle` | Toggles whether the selected placement is enabled. | Disabled placements are excluded from normal projection behavior. |
| `/schem toggle render` | Toggles whether the selected placement is visually rendered. | Hides or shows the projection without deleting the placement. |
| `/schem info` | Shows detailed information about the selected placement. | Includes origin, transform, render state, non-air block count, and bounding box. |
| `/schem clear` | Removes all loaded placements. | Also cancels unfinished loads so old results cannot recreate placements. |

Examples:

```text
/schem list
/schem select 2
/schem toggle render
/schem info
```

### Preview Renderer

| Command | Purpose | Usage Notes |
|------|------|------|
| `/schem renderer stats` | Shows preview cache, geometry and block-entity composite statistics. | Includes per-placement chunk/layer counts, cumulative submitted chunk coverage, build failures, block-entity dispatches, Highlighted records, material variations and skip reasons; submissions do not imply on-screen visibility. With `diagnostics=true`, also writes expected and submitted local chunk origins to `diagnostics/renderer-stats.json` in the mod data directory. |
| `/schem renderer reset` | Releases preview runtime resources. | Preserves placements, transforms and overrides; rebuilds on the next valid render frame. |

Dimension changes release preview caches and keep placements. Leaving a world clears placements and selections; rejoin starts a new session. Actual block entities remain on the existing native display path; visibility follows verification and Y filtering. They are not dispatched while a CPU mesh job owns their fake world.

### Transform Commands

| Command | Purpose | Usage Notes |
|------|------|------|
| `/schem move <dx> <dy> <dz>` | Moves the selected placement by a relative offset. | Positive and negative offsets are both supported. |
| `/schem origin <x> <y> <z>` | Sets the selected placement origin to an exact position. | Use this when you want absolute repositioning instead of relative movement. |
| `/schem rotate <cw90\|ccw90\|r180>` | Rotates the selected placement. | Valid directions are `cw90`, `ccw90`, and `r180`. |
| `/schem mirror <x\|z\|none>` | Mirrors the selected placement on the given axis. | Use `none` to clear mirroring. |
| `/schem reset` | Resets rotation and mirror state for the selected placement. | Returns the placement transform to its default state. |

Examples:

```text
/schem move 5 0 -3
/schem origin 200 70 200
/schem rotate cw90
/schem mirror z
/schem reset
```

### Block Override Commands

| Command | Purpose | Usage Notes |
|------|------|------|
| `/schem block hide <x> <y> <z>` | Hides the projected block at the given world position. | This removes that projected block from the selected placement view. |
| `/schem block set <x> <y> <z> <block>` | Overrides the projected block at the given world position. | Use this to replace one projected block with a different block type. |
| `/schem block clear <x> <y> <z>` | Clears a previously applied block override. | Restores the original projected block behavior at that position. |

Example:

```text
/schem block hide 120 64 90
/schem block set 120 64 90 minecraft:glass
/schem block clear 120 64 90
```

### View Range Commands

| Command | Purpose | Usage Notes |
|------|------|------|
| `/schem view y min <value>` | Sets the minimum visible Y value and enables the filter. | Uses world Y coordinates. Blocks below this Y value are hidden. |
| `/schem view y max <value>` | Sets the maximum visible Y value and enables the filter. | Uses world Y coordinates. Blocks above this Y value are hidden. |
| `/schem view y range <min> <max>` | Sets the full visible Y range and enables the filter. | The visible range is inclusive: `min <= y <= max`. |
| `/schem view y enable` | Re-enables the saved Y range filter. | Keeps the previously stored `min` and `max` values. |
| `/schem view y disable` | Disables Y range filtering. | Does not clear the stored range values. |
| `/schem view y info` | Shows the current Y range filter state. | Displays whether the filter is enabled and the current `min` and `max` values. |

Examples:

```text
/schem view y range 64 96
/schem view y min 80
/schem view y max 120
/schem view y info
/schem view y disable
```

### Selection Commands

| Command | Purpose | Usage Notes |
|------|------|------|
| `/schem pos1` | Sets selection corner 1 to the player's current position. | Useful for quick region marking at your feet. |
| `/schem pos1 <x> <y> <z>` | Sets selection corner 1 to the specified position. | Use when you want exact coordinates. |
| `/schem pos2` | Sets selection corner 2 to the player's current position. | Works the same as `pos1`, but for the second corner. |
| `/schem pos2 <x> <y> <z>` | Sets selection corner 2 to the specified position. | Completes the region when both corners are set. |
| `/schem save <filename>` | Queues progressive capture into a new `.mcstructure` file. | Requires both corners; existing files require confirmation in the Save Selection menu. |
| `/schem save-status` / `/schem save-cancel` | Shows progress or cancels capture. | Unloaded chunks fail explicitly; capture spans multiple ticks. |
| `/schem ui <save\|layers\|verifier\|session>` | Opens a core workflow page. | Also available from the main menu. |
| `/schem verify <start\|layers\|stop\|reset\|info>` | Controls selected placement verification. | `layers` scans the render range; hidden projections can still be verified. |
| `/schem layer mode <all\|single\|range\|below\|above>` | Sets the render layer mode. | Also `layer axis <x\|y\|z>`, `layer boundary <value>` and `layer info`. |
| `/schem selection clear` | Clears the current selection. | Removes both saved corners. |
| `/schem selection mode` | Toggles selection mode on or off. | Selection mode also enables stick-based corner picking. |
| `/schem selection info` | Shows current selection state information. | Displays mode, corners, size, min corner, and total block count when complete. |

Examples:

```text
/schem pos1
/schem pos2 150 72 150
/schem selection info
/schem save room_copy
```

### Selection Mode Interaction

When selection mode is enabled, the source code also supports in-world corner picking with a stick:

- Left click a block with a stick to set `pos1`
- Right click a block with a stick to set `pos2`

## Notes

- The project works with `.mcstructure` files only.
- The runtime creates and uses a `schematics` directory for loading and saving structure files. When no local world path is available, schematic file operations remain unavailable until the path can be resolved.
- The Y range filter is global to the current projection view, not per placement.
- Many commands require a currently selected placement. If nothing is selected, load a schematic first or use `/schem select <id>`.

## Configuration and verification

Selection face color is configured by `app::Config::selection.faceColor`, persisted as `selection.faceColor` in `config.json`. It is an RGBA array in the range `[0, 1]`, defaulting to `[1.0, 1.0, 1.0, 0.10]` (white, 10% opacity). Alpha `0` hides only the faces and keeps the existing selection lines. Apply edits with `/schem config reload`; color/alpha changes rebuild the selection mesh without clearing validation Hulls. Older configuration files receive the default when the field is absent.

`config.json` is created in LeviLamina's mod configuration directory. Typed `app::ConfigStore` supports `snapshot()`, `submit(value)`, `reload()` and `save()`. Consumers use copies. Invalid values retain the last valid configuration; external threads submit values and the client tick applies them.

`visibility.renderDistanceBlocks` defaults to **128 blocks**, shared by projection terrain, validation Hulls, and projection block entities. It measures 3D distance from the rendering camera to the nearest point of each bounding box, not to the placement origin. Set it to `0` for unlimited distance. `visibility.frustumCulling` defaults to `true`; terrain and Hulls use conservative subchunk bounds. Block entities receive distance filtering, not independent frustum/terrain-occlusion filtering. Existing version-1 configuration files receive the new defaults without changing other settings. Apply edits with `/schem config reload`; distance changes do not flush meshes or stop verification.


```powershell
xmake f -a x64 -m release -p windows --target_type=client
xmake
powershell -ExecutionPolicy Bypass -File src-test/run-tests.ps1
```


## Client UI

Release **M**, press **F7**, or run `/schem ui` to open the Litematica-style main menu. Menus use a native scene with C++ drawing and input handling. The package includes the original Litematica/MaLiLib atlases and JE 26.3 button sprites; install the complete `bin/LeviSchematic` directory, including `assets/ui`.

Schematic loading, loaded resources and placements have separate lists. Configuration uses category tabs, search and per-field reset. Valid edits take effect on the client tick and save when leaving configuration or closing the menu. Invalid input and save errors retain the page for correction. Escape cancels capture or a confirmation, then returns to the parent page. The HUD layout editor configures the four mod overlays independently.

Text follows the game language using the existing Simplified Chinese/English translation files under `config/i18n/ui`. Missing translations use built-in defaults. Key overrides retain the existing `ui-keybindings.json` format. Schematics remain `.mcstructure`.

`/schem ui status` reports backend/lifecycle state. `/schem ui layout` exports the last completed frame to the mod data directory's `diagnostics/ui-layout.json`, including viewport, scale, control rectangles, clips and resource UVs. Additional routes include `main`, `load`, `placements`, `placement`, `selection`, `settings`, `keys`, `materials`, `survival` and `tasks`; existing routes remain supported.
