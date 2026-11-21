# LeviSchematic

[中文说明](README_zh.md)

LeviSchematic is a client-side schematic projection mod for Minecraft Bedrock Edition, built on the LeviLamina client SDK and inspired by Litematica. It loads `.mcstructure` and `.litematic` files into the world as translucent projections, helps you build them in survival or creative, checks your build against the schematic, and saves world regions back to `.mcstructure` or `.litematic`.

## Requirements

- Windows x64, Minecraft Bedrock client 1.26.60
- LeviLamina Client 26.60.4
- Install the whole `LeviSchematic` folder (including `assets/ui`) into the `mods` directory

## Features

### Projections

- Load `.mcstructure` or `.litematic` files as translucent ghost blocks, with several placements active at the same time
- Loading runs in the background and appears progressively; large structures do not freeze the game
- Block entities (chests, signs, etc.) and ordinary entities are projected together with the blocks
- Per placement: name, lock, enable/disable, show/hide, origin, move, rotate (90°/180°), mirror (X/Z), reset
- Multi-region placements with per-region transforms, and repeated grids (copies and spacing per axis)
- Render layers: All / Single layer / Range / Below / Above, on the X, Y or Z axis, optionally following the player
- Configurable projection color, tint strength, opacity, render distance and frustum culling

### Verification

- Compares each projected block with the actual world, at full range or only within the current render layers
- Correct blocks are hidden; missing blocks, wrong block types, wrong block states and extra blocks get their own colored markers
- Report page with per-category counts, mismatch groups (expected → actual), per-category highlight toggles and ignorable groups
- Unloaded chunks stay "Unknown" and are never counted as correct

### Survival assistance

- Material list for the selected placement: full volume or current layers only, search, sorting, "remaining only" filter, inventory comparison and CSV export
- Pick block: middle click a projected block to put the matching item in your hand, with configurable hotbar slots
- Block info: hold a key to show the expected and actual block at your crosshair
- Easy Place: places the expected block with the correct state at the aimed position (off by default; states the game cannot reproduce are skipped)
- Placement restriction: blocks placements near a projection that do not match it (expected air, outside the render layers, or the wrong block)
- Material HUD and verifier HUD

### Selections and saving

- Simple mode (one box) and Normal mode (multiple named regions)
- Set corners with the tool, edit coordinates by hand, resize, Grow to surrounding blocks, automatic origin, or create a selection from an existing placement
- Save a selection as a new `.mcstructure`(default) or `.litematic` (optionally including entities); capture runs progressively over several ticks and overwriting requires confirmation
- Multi-region selections are saved as one `.mcstructure` per region plus a `.schemset.json` that ties them together, `.litematic` natively supports multiple regions without requiring special processing.

### Schematic editing

- Edit a loaded schematic directly in the world: remove a projected block, sample a block state, place the sampled state
- Match modes: single block, same state, same type, contiguous line
- Edits belong to the shared resource; save, reload, or "Save As" to fork the selected placement onto a new file

### Creative world tools

Available only in creative mode:

- Paste and Grid Paste a placement into the world, Fill, Replace, Delete, Clone and Move
- Every operation is prepared (pre-checked) first, reviewed, then started; it can be paused, resumed or cancelled
- Scope: whole valid area or current render layers; replacement policy: all cells, target air only, or source non-air only
- Move removes the source only after the target is verified

### Projects and signs

- Projects keep immutable versions of a world selection, with name, description and a work note; any version can be restored as a selection or loaded as a projection
- Copy the front or back text of a projected sign and paste it into the next sign you open normally

### Sessions

- Placements, selections and layer settings are saved per world/server and dimension
- When you rejoin, they are restored only after you confirm on the Session recovery page

## In-game controls

All shortcuts can be remapped on the Key bindings page. Defaults:

| Key | Action |
|---|---|
| `M` (tap and release) | Main menu |
| `M` + `C` | Configuration |
| `M` + `P` | Placement list |
| `M` + `S` | Selection editor |
| `M` + `L` | Material list |
| `M` + `V` | Verifier |
| `M` + `T` | Toggle the tool on/off |
| `Ctrl` + `Alt` + `S` | Save selection |
| Numpad `-` | Selected placement settings |
| Numpad `*` | Selection editor |
| `PageUp` / `PageDown` | Move the current render layer up / down |
| `M` + `PageUp` / `PageDown` | Next / previous layer mode |
| Middle click | Pick a projected block (when not holding the tool) |
| `M` + middle click | Toggle pick block |
| Hold `I` | Show expected/actual block information |
| Right click | Easy Place (when enabled) |

Execute operation, World operations, Projects, Sign copy/paste, Set layer here, Toggle Easy Place and Toggle placement restriction have no default key and can be bound when needed.

### Tool

Hold the tool item (a stick by default, configurable) in the main hand to use these:

| Input | Action |
|---|---|
| `Ctrl` + mouse wheel | Cycle tool mode: Selection → Placement → Edit (creative adds Paste, Grid Paste, Fill, Replace, Delete, Clone, Move) |
| `Ctrl` + `M` | Cycle sub-mode: Corners/Expand in selection, match mode in Edit, replacement policy in Paste |
| Middle click | Select the targeted element: a selection corner, box or region, or a placement |
| `Alt` + mouse wheel | Nudge the selected element one block along the look direction |

Mouse buttons per mode:

- **Selection / Fill / Replace / Delete / Clone / Move**: left click sets corner 1, right click sets corner 2; sneak to use the block in front of the clicked face
- **Placement / Paste**: click a block to move the selected placement's origin there
- **Edit**: left click removes the projected block, middle click samples its state, right click places the sampled state
- **Creative tools**: middle click samples the target block at your crosshair

While the tool is held, an information panel shows the tool mode, the selected file, and the selection corners or the placement origin, size and transforms.

## Menus

The main menu (`M`) opens every page. Use the mouse wheel to scroll and click an entry to run it or edit it; `Esc` cancels the current input or confirmation and then returns to the parent page.

| Page | What you can do |
|---|---|
| Placements | List all placements, select, enable/disable, open Load schematics |
| Load schematics | Browse available `.mcstructure` files, load one, watch and cancel load tasks |
| Loaded schematics | Shared resources: save/reload edits, Save As, unload together with their placements |
| Selected placement | Name, lock, enable, render, actors, origin, move, rotation, mirror, regions, repeated grid, remove, clear all |
| Selection editor | Simple/Normal mode, regions, corner coordinates, Grow, automatic origin, selection from placement, clear |
| Save selection | File name, include entities, save new / overwrite, progress and cancel |
| Render layers | Layer mode, axis, layer/minimum/maximum, follow player, which bound moves when stepping, set to player layer |
| Verifier | Pick a placement, start (full or layers), stop, reset, report, highlight and ignore groups |
| Materials | Material list with search, sorting, paging, remaining-only filter and CSV export |
| Survival assistance | Pick block, Easy Place, hold to place, placement restriction, swap delay, hotbar slots, HUD switches |
| Edit schematic | Enable edit mode, match mode, add adjacent, replacement block, save/cancel edits |
| World operations | Operation, scope, replacement policy, sampling, clone/move offset, prepare, start, pause, resume, cancel |
| Projects | Create projects, snapshot versions, browse versions, restore a selection, paste a version |
| Sign text | Copy front/back, paste into the next opened sign |
| Task manager | Load, save, world operation and project tasks in one place |
| Session recovery | Restore the saved session for the current dimension or keep the current state |
| Key bindings | Click a binding, hold the new key or chord and release to save; the arrow resets it, `X` clears it |
| Configuration | Category tabs, search and per-field reset; changes apply on the next tick and save when you leave |

### Configuration

- Visibility: render distance, frustum culling
- Projection: color and tint strength, opacity
- Verification: colors for missing, wrong-type, wrong-state and extra blocks, always show missing / extra markers, outline expansion
- Selection face color, tool item, HUD on/off
- Survival assistance options and performance budgets
- Colors are entered as `R, G, B, A%`, for example `255, 128, 0, 50%`

### HUD layout

Configuration → HUD layout lets you arrange the four HUD panels (projection info, target block, materials, verifier): drag each panel, scale it from 50% to 200% with the slider or a number, or reset one or all of them. Panels that are currently inactive show sample content while editing. The layout is saved when you leave the page.

## Language

The UI follows the game language and ships with English and Simplified Chinese.

## Building

```powershell
xmake f -a x64 -m release -p windows --target_type=client
xmake
```

The packaged mod is written to `bin/LeviSchematic`.

## License

- LGPL-3.0.
- The bundled Litematica/MaLiLib textures are LGPL-3.0; see `assets/ui` for sources and licenses.
- The JE-to-BE block mapping table is sourced from [GeyserMC](https://github.com/GeyserMC/mappings) and is MIT License; see `assets/mappings` for the license.