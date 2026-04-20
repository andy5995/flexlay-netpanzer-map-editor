# Autotile Sidecar Format

Autotiling lets the editor automatically pick the correct transition tile variant
when you paint terrain, producing smooth edges instead of hard staircase steps.

## How it works

When you paint with a tile that belongs to an autotile group the editor:

1. Looks at the 8 neighbours of the painted cell to see which ones are in the
   same group.
2. Builds an 8-bit **bitmask** from those neighbours (blob-tileset convention).
3. Looks up the correct tile variant in the group's bitmask table and places it.
4. Re-evaluates all 8 neighbours and updates their variants too, so edges stay
   consistent as you drag.

Undo restores every tile that was changed — the painted tile and all its updated
neighbours — in a single step.

## Sidecar file

The editor searches for `<tileset-stem>.autotile.json` in order:

1. Same directory as the `.tls` file
2. `<app>/share/netpanzer-editor/autotile/` (installed AppImage path)
3. `<app>/data/autotile/` (beside the binary)
4. `<app>/../data/autotile/` (running from a build subdirectory)

If the file is found the **Autotile** toggle in the Tools menu (and toolbar)
becomes active.  Example layout:

    data/wads/summer12mb/SummerDay/summer12mb.tls
    data/autotile/summer12mb.autotile.json     ← found via path 4 above

## JSON schema

Each group must have a `member_tiles` array listing every tile ID that belongs
to the terrain.  The editor uses this to identify which painted tiles should
trigger autotiling and to compute neighbour bitmasks.

The `bitmask` object maps bitmask values (as strings) to tile IDs.  Groups
without a `bitmask` object (stamp-type groups such as buildings or mountains)
are recognised as members but are not subject to autotile replacement.

```json
{
  "groups": [
    {
      "name": "grass",
      "member_tiles": [8097, 8098, 8099],
      "bitmask": {
        "0":   10,
        "1":   11,
        "4":   12,
        "5":   13,
        "16":  14,
        "17":  15,
        "20":  16,
        "21":  17,
        "64":  18,
        "65":  19,
        "68":  20,
        "69":  21,
        "80":  22,
        "81":  23,
        "84":  24,
        "85":  25
      }
    }
  ]
}
```

`"groups"` is an array so a single `.tls` file can define multiple independent
terrain types (e.g. grass, water, road).

### Bitmask bit layout

Bits are assigned clockwise from the top.  Diagonal bits are only counted when
**both** adjacent cardinal bits are also set (standard blob-tileset convention,
which reduces the 256 possible combinations to 47 distinct visual states):

```
bit 7 (128)  bit 0 (  1)  bit 1 (  2)
bit 6 ( 64)    TILE       bit 2 (  4)
bit 5 ( 32)  bit 4 ( 16)  bit 3 (  8)
```

| Bit | Value | Direction |
|-----|-------|-----------|
|  0  |   1   | N         |
|  1  |   2   | NE        |
|  2  |   4   | E         |
|  3  |   8   | SE        |
|  4  |  16   | S         |
|  5  |  32   | SW        |
|  6  |  64   | W         |
|  7  | 128   | NW        |

If the exact bitmask is not in the table the editor falls back to bitmask `255`
(the fully-surrounded tile).  If `255` is also absent the tile is left unchanged.

## Deriving bitmask mappings from an existing tileset

If you have a tileset whose bitmask data was never recorded (such as
`summer12mb.tls`), use the derivation tool to infer mappings from tile imagery:

```sh
python3 tools/derive_autotile_bitmasks.py \
    --tls  /path/to/summer12mb.tls \
    --act  /path/to/netp.act \
    --json data/autotile/summer12mb.autotile.json \
    --out  /tmp/summer12mb_derived
```

The tool selects the most-uniform tile per group as the bitmask-255 reference,
compares each tile's 8 directional edge zones against that reference, and
auto-calibrates a match threshold to maximise canonical bitmask coverage.
Review the `.bitmask_report.txt` output and adjust `--threshold` if needed.

Groups that contain pre-drawn artistic stamps (mountains, buildings) rather than
autotile terrain variants should have their `bitmask` entries removed — the tool
preserves groups that already have more than 5 entries and skips them.
