# LeviSchematic

[中文说明](README_zh.md)

> **Notice: almost all of this project's features and UI design come from the Java Edition mod [Litematica](https://github.com/maruohon/litematica) by masa.** LeviSchematic ports them to Bedrock Edition on top of LeviLamina; the menu layouts, button behaviour, info HUD, material list and file metadata all follow Litematica. Thanks to the authors and contributors of Litematica and MaLiLib.

LeviSchematic is a client-side schematic projection mod for Minecraft Bedrock Edition, built on the LeviLamina client SDK and inspired by Litematica. It loads `.levischematic`, `.litematic` and `.mcstructure` files into the world as translucent projections, helps you build them in survival or creative, checks your build against the schematic, and saves world regions back to `.levischematic` (default), `.litematic` or `.mcstructure`.

## Requirements

- Windows x64, Minecraft Bedrock client 1.26.60
- LeviLamina Client 26.60.4
- Install the whole `LeviSchematic` folder (including `assets/ui`) into the `mods` directory

## Features

### Projections

- Load `.levischematic`, `.litematic` or `.mcstructure` files as translucent ghost blocks, with several placements active at the same time
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

- Material list for the selected placement, an unloaded schematic file (read without loading it) or the blocks inside a world selection; sortable columns, hide available, multiplier, ignored entries, item icons and TXT/CSV/JSON export; liquids are counted as buckets (including waterlogged water)
- Pick block: middle click a projected block to put the matching item in your hand, with configurable hotbar slots
- Block info: hold a key to show the expected and actual block at your crosshair
- Easy Place: places the expected block with the correct state at the aimed position (off by default; states the game cannot reproduce are skipped)
- Placement restriction: blocks placements near a projection that do not match it (expected air, outside the render layers, or the wrong block)
- Material HUD and verifier HUD

### Selections and saving

- Simple mode (one box) and Normal mode (multiple named regions)
- Set corners with the tool, edit coordinates by hand, resize, Grow to surrounding blocks, automatic origin, or create a selection from an existing placement
- Save a selection as a new `.levischematic` (default, used when no extension is given), `.litematic` or `.mcstructure` (optionally including entities); capture runs progressively over several ticks and overwriting requires confirmation
- `.levischematic` is a single file that keeps every sub-region together with Litematica-style metadata (name, author, times, region count, block count, size), so the schematic browser shows its details without unpacking it. `.litematic` also keeps sub-regions; saving a multi-region selection as `.mcstructure` merges the regions into one structure and asks for confirmation first

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

The menus are rebuilt after the Litematica screens. The main menu (`M`) opens every page and the title shows a breadcrumb (`LeviSchematic vX Main Menu => … => …`). Scroll with the mouse wheel and click to run an entry; hovering a button shows its description; cycle buttons go forward on left click and back on right click; `±` buttons take larger steps with `Shift`, `Ctrl` or `Alt`; `Esc` cancels the current input or confirmation and then returns to the parent page.

| Page | What you can do |
|---|---|
| Main menu | Left column: placements, loaded schematics, load schematics, area editor, area selection browser, selection mode; right column: configuration, schematic manager, task manager, projects; below them the mod's own features (edit schematic, world operations, sign text, session recovery); tool mode and tool switch at the bottom |
| Load schematics | Directory navigation (root, up, new folder, open in the system file browser) and search; info panel with name, author, created/modified time, region count, block count/volume and size; "Create placement" checkbox (unchecked loads into memory only, `Shift` loads it disabled); material list, rename schematic, rename file |
| Schematic manager | Rename schematic, change author; convert between `.levischematic`, `.litematic` and `.mcstructure`; rename, copy and delete files |
| Loaded schematics | Create placement (`Shift` for disabled), save to file / Save As, reload, unload |
| Placements | Each row: name (green enabled / red disabled), lock icon, configure, enable, remove (locked ones need `Shift`) |
| Placement configuration | Name and rename; sub-region list (configure, enable, all on/off); enable, render, lock, enclosing box, ignore entities; origin XYZ (input, `±`, axis lock), move to player, rotate, mirror, reset sub-regions; footer links to material list, verifier and repeated grid |
| Area selection browser | Selection files `area_selections/*.json`: create, create from placement, deselect; each row configure, copy, rename, delete |
| Area editor | Selection mode, corner mode, manual origin; selection / sub-region name; XYZ input and `±` for the corners and origin, move to player; save schematic, analyze area; sub-region list; auto grow, expand/contract, clear |
| Save schematic | File name (the extension picks the format, `.levischematic` when omitted), include entities, save new / overwrite, progress and cancel |
| Material list | Refresh, list type (all / render layers), hide available, HUD, multiplier; sortable header; hover shows `N = a × 64 + b = c shulker boxes`; ignore, clear cache, TXT/CSV/JSON export |
| Verifier | Pick a placement, start (full or layers), stop, reset, report, highlight and ignore groups |
| Edit schematic | Enable edit mode, match mode, add adjacent, replacement block, save/cancel edits |
| World operations | Operation, scope, replacement policy, sampling, clone/move offset, prepare, start, pause, resume, cancel |
| Projects | Create projects, snapshot versions, browse versions, restore a selection, paste a version |
| Sign text | Copy front/back, paste into the next opened sign |
| Task manager | Load, save, world operation and project tasks in one place |
| Session recovery | Restore the saved session for the current dimension or keep the current state |
| Configuration | Tabs: All / Generic / Info HUD / Visuals / Colors / Hotkeys / Render layers; every field has a description and a reset button |

### Configuration

- Visibility: render distance, frustum culling
- Projection: color and tint strength, opacity
- Verification: colors for missing, wrong-type, wrong-state and extra blocks, always show missing / extra markers, outline expansion
- Selection face color, tool item, HUD on/off
- Survival assistance options and performance budgets
- Colors are entered as `#AARRGGBB` (`AA` is the opacity); a swatch next to the field shows the current color

### Hotkeys

In Configuration → Hotkeys, click a binding, hold the new key or chord and release to save; `Esc` clears the binding. Bindings that differ from the default are shown in gold. The settings icon next to a binding opens its advanced settings on left click and resets them on right click: activate on (press / release / both), context (in game / in screens / anywhere), allow empty, allow extra keys, order sensitive, exclusive and cancel further processing.

### Info HUD

While the tool is held, the bottom-left corner shows information for the current tool mode in the Litematica style: the selection name, corner mode, sub-region and both corners in selection modes; the selected placement, origin, size, sub-region count and modified sub-regions in placement modes; the replacement policy and similar settings for creative tools. Segments are colored, every line has a translucent background, and the render layer setting is shown as well.

### HUD layout

Configuration → Edit HUD layout lets you arrange the four HUD panels (projection info, target block, materials, verifier): drag each panel, scale it from 50% to 200% with the slider or a number, or reset one or all of them. Panels that are currently inactive show sample content while editing. The layout is saved when you leave the page.

## Schematic file formats

| Extension | Load | Save |
|---|---|---|
| `.levischematic` | Yes, multiple regions | Default (used when no extension is given) |
| `.litematic` | Yes, multiple regions | Yes, multiple regions |
| `.mcstructure` | Yes, single structure | Yes; a multi-region selection asks first and is merged into one structure (gaps become structure void, later regions overwrite earlier ones where they overlap) |

`.levischematic` is this mod's single-file format and replaces the old multi-file layout (one `.mcstructure` per region plus a `.schemset.json` manifest):

- The file starts with the magic `LVSCHEM\0`, a container version and an **uncompressed** JSON header, followed by the region data.
- The header's `metadata` matches Litematica's metadata: name, author, description, created/modified time, region count, block count, total volume and enclosing size; `regions` records each sub-region's id, name, relative offset and size.
- Each region is a vanilla little-endian NBT mcstructure, gzip-compressed, with its raw length and CRC32; any mismatch while reading is reported as a corrupt file.
- The schematic browser reads only the header to show details without decompressing any region; renaming or changing the author in the schematic manager rewrites only the header.
- Files are written atomically, so a cancelled or failed save never leaves a partial file.

See `docs/levischematic文件格式说明.md` for the byte layout.

## Language

The UI, HUD, chat messages and command output follow the game language and ship with English and Simplified Chinese. All text lives in `assets/lang/<locale>.json` (installed to `<mod>/assets/lang`); to add a language, copy `en_US.json` to the game's locale code (for example `ja_JP.json`) and translate the values. Files placed in `<config>/lang/` override individual keys (the old auto-generated `<config>/i18n/ui/` files are no longer read).

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