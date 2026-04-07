# LeviShematic

[中文说明](README_zh.md)

## Introduction

LeviShematic is a Bedrock client-side schematic projection mod built around `.mcstructure` files.
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
- Refreshes projection chunks after placement edits
- Tracks expected blocks from loaded placements
- Compares projected blocks with actual world blocks
- Uses different projection colors for mismatched block types and mismatched block properties

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
| `/schem load <filename> <x> <y> <z>` | Loads a schematic at the specified world position. | This creates a new placement and prints its placement ID. |

Examples:

```text
/schem load house
/schem load tower 100 64 -32
```

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
| `/schem clear` | Removes all loaded placements. | Clears the full placement list at once. |

Examples:

```text
/schem list
/schem select 2
/schem toggle render
/schem info
```

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

### Selection Commands

| Command | Purpose | Usage Notes |
|------|------|------|
| `/schem pos1` | Sets selection corner 1 to the player's current position. | Useful for quick region marking at your feet. |
| `/schem pos1 <x> <y> <z>` | Sets selection corner 1 to the specified position. | Use when you want exact coordinates. |
| `/schem pos2` | Sets selection corner 2 to the player's current position. | Works the same as `pos1`, but for the second corner. |
| `/schem pos2 <x> <y> <z>` | Sets selection corner 2 to the specified position. | Completes the region when both corners are set. |
| `/schem save <filename>` | Saves the selected region as a `.mcstructure` file. | Requires both selection corners to be set first. |
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
- The runtime creates and uses a `schematics` directory for loading and saving structure files.
- Many commands require a currently selected placement. If nothing is selected, load a schematic first or use `/schem select <id>`.
