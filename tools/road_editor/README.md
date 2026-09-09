# Playerbot V2 — Standalone Road / Metadata Editor

A single-file HTML application for efficient **bulk** annotation of
roads, cities, villages, hubs, danger zones, vendors, mailboxes, etc.
No install, no build step, no server roundtrip.

## Why this exists

The in-game `.playerbot meta add road` GM command is fine for a handful
of points, but annotating Teldrassil's road network point-by-point
in-game would take days. This editor lets you draw a polyline on a
zone screenshot and emit a fully-interpolated CSV in seconds.

## Recommended workflow: navmesh PNG + sidecar (no manual calibration)

1. **Dump a map's navmesh as a top-down PNG** using the
   `mmap_world_dump` tool (built alongside `worldserver`):

   ```powershell
   M:\PlayerbotServer\mmap_world_dump.exe `
       M:\PlayerbotServer\mmaps `
       1 `
       M:\WorldofWarcraft\editor_dump\kalimdor `
       2.0
   ```
   - Arg 1: directory containing `<mapId>.mmap` + `*.mmtile` files.
   - Arg 2: WoW map id (`0` Eastern Kingdoms, `1` Kalimdor, `530`
     Outland, `571` Northrend, …).
   - Arg 3: output basepath. Emits `<base>.png` + `<base>.json`.
   - Arg 4 (optional): yards per pixel (default `2.0`). Lower = sharper
     but bigger. Capped at 16384 px per axis.

2. **Open** `road_editor.html` in any modern browser (double-click).

3. **Drop the PNG and the JSON together** onto the drop zone (or pick
   both via the file picker — Ctrl-click to select both). The editor
   parses the sidecar's transform, auto-fills the Map ID, and skips
   calibration entirely. The header chip flips to `auto (map N)`.

4. **Set Kind, Radius, Step**, then click or click-drag to annotate.
   Every click yields exact TC world coords because the PNG was
   generated with a known per-pixel world transform.

5. **Export CSV** when done. Save as
   `M:\WorldofWarcraft\world_metadata.csv`.

## Legacy workflow: zone screenshot + manual calibration

Use this only if you don't have a navmesh dump for the target map.

1. **Open** `road_editor.html` in any modern browser.
2. **Drop a zone screenshot** (PNG / JPG). In-game world map
   screenshots work; so do `data/maps`-derived PNGs.
3. **Calibrate** by clicking two known points on the image (the editor
   stores their pixel positions), then entering their TC world
   coordinates `(x, y)` in the left panel. Pick anchors as far apart
   as possible for accuracy — corners of the map work best.
   - Stand on a known point in-game and run `.gps` (or read your
     unit-frame coords) to get the world `(x, y)` for that pixel.
   - Repeat for a second point.
   - Click **Apply calibration**. The editor auto-detects axis-aligned
     vs 90°-rotated screenshots.
4. **Set Map ID, Kind, Radius, Step**.
   - `Map ID`: e.g. `1` for Kalimdor (where Teldrassil lives).
   - `Kind`: click one of the colored pills (road / city / village /
     hub / danger / vendor / mailbox / innkeeper / crossroad / other).
   - `Radius`: how big a circle the waypoint covers (yards). The
     `mmaps_generator` tags any polygon whose centroid is within
     `radius` as `NAV_AREA_ROAD` (for Road kind).
   - `Step`: how far apart auto-interpolated polyline points are.
     12-15y is typical for roads.
5. **Annotate**:
   - **Click** to drop a single point.
   - **Click-and-drag** to draw a polyline that auto-interpolates one
     point every `Step` yards. (Or use Shift-click sequence.)
   - **Right-click a placed point** to delete it (the closest within
     20px).
   - **Mouse wheel** zooms (centered on cursor). **Middle-drag** pans.
   - **Z** undoes; **Y** (or **Shift-Z**) redoes.
6. **Export** when done:
   - Click **Export CSV** to download `world_metadata.csv`.
   - OR click **Copy to clipboard** and paste it into any text editor.
   - Save the file as `M:\WorldofWarcraft\world_metadata.csv`.

## Putting the CSV to use

Two consumers read this exact same file:

1. **`mmaps_generator`** at the next regen — auto-loads
   `<input>/world_metadata.csv` and tags polygons within each Road
   point's radius as `NAV_AREA_ROAD`.

   ```powershell
   # Stop worldserver
   M:\PlayerbotServer\mmaps_generator.exe 1 `
       --input M:\WorldofWarcraft `
       --output M:\WorldofWarcraft
   # Restart worldserver
   ```

2. **`worldserver`** at boot — loads via `WorldMetadataStore` for bot
   situational awareness (in_city, in_village, in_danger_zone,
   walk_to_known_vendor, walk_to_known_hub, walk_to_known_mailbox,
   walk_to_innkeeper_rebind fallback).

   To pick up edits without restart, the GM command
   `.playerbot meta reload` will re-read the CSV into the in-memory
   cache.

## Re-editing an existing CSV

Use **Import existing CSV** (under the import details panel). Paste
the CSV contents and the editor will reconstruct points on the canvas
(after calibration). Then you can add/delete points and re-export.

## Output format

Identical to the format `.playerbot meta export` produces:

```
# playerbot_v2_world_metadata export from road_editor.html
# columns: id,map_id,zone_id,kind,kind_name,x,y,z,radius,label,notes
0,1,0,1,road,9986.500,2376.250,1322.700,12.00,Shadowglen exit road,
0,1,0,1,road,9970.250,2380.100,1322.900,12.00,Shadowglen exit road,
...
```

`id`, `zone_id`, and `notes` are blank/zero on export — the worldserver
assigns real `id`s when loading via `.playerbot meta reload`.

## Recommended workflow for Teldrassil

1. Take an in-game screenshot of the Teldrassil world map (open the
   map, press the screenshot key). Or use `M:\WorldofWarcraft\maps\*`
   tiles stitched together.
2. Open `road_editor.html`. Drop the screenshot.
3. In-game, stand on a known landmark (e.g. the Shadowglen flight
   master) and note your `(x, y)`. Click the corresponding pixel on
   the screenshot as Anchor A; enter the coords.
4. Repeat for Anchor B as far away as possible (e.g. Rut'theran
   Village dock). Click **Apply calibration**.
5. Set Kind=road, Step=12y, Radius=12y.
6. Click-and-drag along the Shadowglen → Dolanaar road. Release.
7. Repeat for Dolanaar → Rut'theran, Dolanaar → Lake Al'Ameth,
   Dolanaar → Pools of Arlithrien, Darnassus inner ring, etc.
8. Export CSV → save as `M:\WorldofWarcraft\world_metadata.csv`.
9. Regen mmaps (map 1 only is enough).
10. Restart worldserver. Whisper any bot `roadstats 1` — expect
    non-zero road polys; bots in Teldrassil now prefer the annotated
    routes.

## Limitations

- The 2-anchor calibration assumes the screenshot is axis-aligned with
  the world (or rotated 90°). True for the standard WoW world map.
  Skewed/oblique screenshots will not calibrate accurately — pick a
  proper top-down map source.
- The `z` coordinate defaults to the value in the **Z height** input.
  `mmaps_generator` only uses `x/y` for the radius gate (`z` is
  ignored for road tagging), so leaving it at 0 is fine for roads.
  For Innkeeper/Mailbox anchors used by bot rules, set `z` to the
  actual world z if you want pinpoint walk-to behavior; otherwise the
  rules path-find to `(x, y, ground_z)` which is usually correct.
- Combining import + new points: imported points are appended; the
  editor does not merge by id. Run an import on an empty canvas before
  adding new points if you want a clean state.

## Files / pointers

| Piece | File |
|-------|------|
| The editor | `tools/road_editor/road_editor.html` |
| CSV format | matches `.playerbot meta export` |
| mmaps_generator consumer | `src/common/mmaps_common/Generator/RoadOverrides.cpp` |
| Bot store | `src/modules/PlayerbotV2/World/WorldMetadata.cpp` |
| In-game commands | `.playerbot meta` in `cs_playerbot_v2.cpp` |
| Subsystem doc | `src/modules/PlayerbotV2/docs/WORLD_METADATA_EDITOR.md` |
