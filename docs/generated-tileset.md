# Generated Tileset

`data/wads/generated/Default/generated.tls` is a procedurally generated tileset
bundled with the editor so it can be used without the original netPanzer game
data installed.

> **Note:** Maps made with this tileset cannot be played in the netPanzer engine,
> which only supports the summer12mb tileset.  Use it for editor testing and
> development, or when game data is not available.

## Contents

| Group    | Tile IDs | Type |
|----------|----------|------|
| grass    | 0–46     | blob-8 autotile (47 bitmask variants) |
| mountain | 47–93    | blob-8 autotile |
| water    | 94–140   | blob-8 autotile |
| sand     | 141–187  | blob-8 autotile |
| road     | 188–234  | blob-8 autotile |
| outpost  | 235–810  | stamp (embedded from summer12mb) |

The outpost group (tiles 235–810) is the summer12mb industrial compound art,
repalettized to fit the generated palette.  Load `data/stamps/outpost.stamp.json`
in the stamp panel to place the full 32×18 tile structure in one click.

## Palette

`generated.act` (beside the `.tls`) defines the colour palette.  The editor's
tileset loader checks for a same-stem `.act` file before falling back to
`netp.act`, so the generated tileset renders with its own colours regardless of
where the editor is run from.

Palette slot layout:

| Slots   | Contents |
|---------|----------|
| 0–7     | grass shades |
| 8–15    | mountain shades |
| 16–23   | water shades |
| 24–31   | sand shades |
| 32–39   | road shades |
| 40–47   | shared neutral tones |
| 48–63   | noise grain variants |
| 64–255  | colours from embedded source tiles (outpost art) |

## Regenerating

Run the generator script when visuals need to change.  Pass `--source-tls` and
`--embed` to include art from an existing tileset:

```sh
python3 tools/generate_tileset.py \
    --out-dir data/wads/generated \
    --source-tls ~/src/netpanzer/data/wads/summer12mb/SummerDay/summer12mb.tls \
    --source-act ~/src/netpanzer/data/wads/netp.act \
    --embed 9541 10117 outpost

cp data/wads/generated/generated.autotile.json data/autotile/
```

Then commit the updated binary files and autotile JSON together.

Tile IDs are stable as long as `TERRAIN_ORDER` in the script is not reordered
and no terrain is inserted before an existing one (appending new terrains or
embed groups at the end is safe).  Existing maps do not require any editor code
changes after regeneration.

To embed additional building groups (e.g. houses, tiles 0–1151):

```sh
python3 tools/generate_tileset.py \
    --out-dir data/wads/generated \
    --source-tls ... --source-act ... \
    --embed 9541 10117 outpost \
    --embed 0    1152  buildings
```

Up to 192 unique colours from each embedded range are merged into palette slots
64–255 using nearest-colour remapping.
