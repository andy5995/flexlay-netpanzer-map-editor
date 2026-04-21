# Adding Tiles to a Tileset

This guide covers adding new 32×32 tiles to an existing netPanzer `.tls`
tileset file (e.g. `summer12mb.tls`).

## Prerequisites

- GIMP (any recent version)
- Python 3 with Pillow and numpy: `pip install Pillow numpy`

## Step 1 — Draw the tile in GIMP

1. **File → New…** — set size to **32 × 32** pixels.
2. Draw your tile using colours that match the surrounding tileset tiles.
   You do not need to worry about palette management — the script handles
   colour quantization automatically.

## Step 2 — Export as PNG

**File → Export As…**, save as a `.png` file.  Leave all export options at
their defaults and click **Export**.

The file can be RGB, RGBA, or palette-indexed — all modes are accepted.

## Step 3 — Append the tile to the tileset

```sh
python3 tools/add_tile.py \
    --tls /path/to/summer12mb.tls \
    --tile mytile.png
```

The script:
- Quantizes the image to the colours used by the tileset, using the same
  `netp.act` palette that the editor and game use for rendering.
- Backs up the original `.tls` as `.tls.bak` before making any changes.
- Inserts a 3-byte tile header and appends 1024 bytes of pixel data.
- Prints the new tile's ID — use this ID in the tile browser or a stamp JSON.

Use `--dry-run` to preview without writing:

```sh
python3 tools/add_tile.py --tls summer12mb.tls --tile mytile.png --dry-run
```

### Options

| Flag | Default | Meaning |
|------|---------|---------|
| `--attrib 0` | 0 | `0` = normal, `1` = impassable (walls, deep water) |
| `--move-value 0` | 0 | Movement cost hint used by the netPanzer engine |

## Replacing an existing tile

To overwrite a tile at a specific ID without changing the tile count:

```sh
python3 tools/replace_tile.py \
    --tls /path/to/summer12mb.tls \
    --id 9467 \
    --tile newtile.png
```

The same quantization and backup logic applies.

## Notes on the `.tls` format

Each tile is stored as 1024 bytes of palette indices (32 × 32, row-major,
top-left to bottom-right).  The `netp.act` file (ships with netPanzer, found
in `data/wads/`) contains the 256-colour palette used for rendering.

See [docs/map-format.md](map-format.md) for a full description of the `.tls`
binary layout.
