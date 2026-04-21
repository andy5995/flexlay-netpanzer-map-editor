# Adding Tiles to a Tileset

This guide covers adding new 32×32 tiles to an existing netPanzer `.tls`
tileset file (e.g. `summer12mb.tls`).

## Prerequisites

- GIMP (any recent version)
- Python 3 with Pillow: `pip install Pillow`
- The netPanzer palette file: `data/wads/netp.act` (ships with netPanzer)
- The target `.tls` file (e.g. from `netpanzer/data/wads/summer12mb.tls`)

## Step 1 — Import the palette into GIMP

1. Open GIMP and go to **Windows → Dockable Dialogs → Palettes**.
2. In the Palettes panel, right-click → **Import Palette…**
3. Set source to **Palette file**, navigate to `netp.act`, click **Import**.

The palette will appear in the list as "netp" (or the filename stem).

## Step 2 — Draw the tile

1. **File → New…** — set size to **32 × 32** pixels.
2. Draw your tile using colours from the netp palette.
   - To paint with palette colours: open **Windows → Dockable Dialogs → Palettes**,
     double-click a swatch to set the foreground colour.

## Step 3 — Convert to indexed mode

1. **Image → Mode → Indexed…**
2. Select **Use custom palette**, choose **netp** from the dropdown.
3. Uncheck **Remove unused colors from colormap**.
4. Click **Convert**.

## Step 4 — Export as PNG

**File → Export As…**, save as a `.png` file.  
GIMP will preserve the indexed (palette) mode in the exported file.

## Step 5 — Append the tile to the tileset

```sh
python3 tools/add_tile.py \
    --tls /path/to/summer12mb.tls \
    --tile mytile.png
```

The script:
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

## Notes on the `.tls` format

Each tile is stored as 1024 bytes of palette indices (32 × 32, row-major,
top-left to bottom-right).  The palette is embedded in the `.tls` header;
`netp.act` is a standalone copy of the same 256-colour table.

See [docs/map-format.md](map-format.md) for a full description of the `.tls`
binary layout.
