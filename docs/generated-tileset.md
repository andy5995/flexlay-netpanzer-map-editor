# Generated Tileset

`data/wads/generated/Default/generated.tls` is a procedurally generated tileset
bundled with the editor for use when you don't have the original netPanzer game
data installed.

## Contents

5 terrain types × 47 blob-8 bitmask variants = 235 tiles total:

| Terrain  | Tile IDs  |
|----------|-----------|
| grass    | 0–46      |
| mountain | 47–93     |
| water    | 94–140    |
| sand     | 141–187   |
| road     | 188–234   |

Full autotile coverage is provided by `data/autotile/generated.autotile.json`,
which is loaded automatically when the editor opens a map using this tileset.

## Palette

`generated.act` (beside the `.tls`) defines the colour palette.  The editor's
tileset loader checks for a same-stem `.act` file before falling back to the
game's `netp.act`, so the generated tileset renders with its own colours
regardless of where the editor is run from.

## Regenerating

Run the generator script whenever the visuals need to change:

```sh
python3 tools/generate_tileset.py --out-dir data/wads/generated
cp data/wads/generated/generated.autotile.json data/autotile/
```

Then commit both the new binary files and the updated autotile JSON together.
Tile IDs are stable as long as `TERRAIN_ORDER` in the script is not reordered
and no terrain is inserted before an existing one (appending new terrains is
safe).  Existing maps do not require any editor code changes after regeneration.
