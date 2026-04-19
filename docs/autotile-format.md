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

Place a JSON file alongside your `.tls` file using the same stem:

    summer12mb.tls          ← tileset
    summer12mb.autotile.json  ← sidecar (auto-discovered)

The editor looks for `<tileset-stem>.autotile.json` in the same directory as the
`.tls` file whenever a tileset is loaded.  If the file is found the **Autotile**
toggle in the Tools menu (and toolbar) becomes active.

## JSON schema

```json
{
  "groups": [
    {
      "name": "grass",
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

If the exact bitmask is not in the table the editor falls back to bitmask `0`
(the fully-isolated tile).  If bitmask `0` is also absent the tile ID selected
in the palette is placed unchanged.
