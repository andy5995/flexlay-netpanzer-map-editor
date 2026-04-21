# netPanzer Map Format

A netPanzer map consists of up to three files that share a common base name:

| File | Required | Content |
|------|----------|---------|
| `<name>.npm` | yes | Binary tile grid |
| `<name>.opt` | no | Outpost/objective locations |
| `<name>.spn` | no | Spawn point locations |

All multi-byte integers are **little-endian**.

---

## .npm — Binary Tile Map

### Header (1610 bytes)

| Offset | Size | Type | Field |
|--------|------|------|-------|
| 0 | 64 | `char[]` | ID header string (e.g. `"Copyright PyroSoft Inc."`, null-padded) |
| 64 | 2 | `uint16` | Map ID |
| 66 | 256 | `char[]` | Map name (null-terminated, null-padded) |
| 322 | 1024 | `char[]` | Description (null-terminated, null-padded) |
| 1346 | 2 | `uint16` | Width in tiles |
| 1348 | 2 | `uint16` | Height in tiles |
| 1350 | 256 | `char[]` | Tileset filename (e.g. `"summer12mb.tls"`, null-padded) |
| 1606 | 2 | `uint16` | Thumbnail width in pixels (0 if absent) |
| 1608 | 2 | `uint16` | Thumbnail height in pixels (0 if absent) |

### Tile Data

Immediately follows the header at offset **1610**.

```
uint16  tiles[width * height]
```

Tiles are stored in row-major order (left to right, top to bottom). Each value is a zero-based index into the tileset. The tile array is `width × height × 2` bytes.

### Thumbnail (optional)

Immediately follows the tile data.

```
uint8  thumbnail[thumbnail_width * thumbnail_height]
```

Each byte is a palette index (same 256-colour palette as the `.tls` file). May be absent; check `thumbnail_width == 0 || thumbnail_height == 0`.

### Minimum valid file size

`1610 + width × height × 2` bytes (no thumbnail).

---

## .opt — Outpost / Objective File

Plain text. One block per outpost, preceded by a count header.

```
ObjectiveCount: <N>

Name: <outpost name>
Location: <tile-x> <tile-y>

Name: <outpost name>
Location: <tile-x> <tile-y>
```

- `ObjectiveCount` gives the number of outpost blocks that follow.
- `Name` is a human-readable label for the outpost.
- `Location` is the outpost marker position in **tile coordinates** (not pixels).
- Blank lines between blocks are conventional but not required.
- If `Name` is missing for an entry, parsers should synthesise one (e.g. `Outpost#N`).

### Capture pad

netPanzer computes the capturable area from the outpost tile position as follows (using pixel coordinates where one tile = 32 × 32 px):

```
marker_pixel_x = tile_x * 32 + 16   // tile centre
marker_pixel_y = tile_y * 32 + 16

pad_centre_x = marker_pixel_x + 224
pad_centre_y = marker_pixel_y + 48

capture_box = pad_centre ± (48, 32) px
```

The editor draws this pad as a visual overlay when the **Place Outpost** tool is active.

---

## .spn — Spawn Point File

Plain text. One `Location` line per spawn point, preceded by a count header.

```
SpawnCount: <N>
Location: <tile-x> <tile-y>
Location: <tile-x> <tile-y>
```

- `SpawnCount` gives the number of spawn points that follow.
- `Location` is the spawn point position in **tile coordinates**.
- Spawn points have no name field.

### Quirk: no-newline format

Some `.spn` files in the wild omit newlines between tokens, concatenating everything onto a single line:

```
SpawnCount: 3Location: 5 10Location: 30 40Location: 99 7
```

Parsers must normalise this by inserting a newline before each `Location:` token before parsing.

---

## .tls — Tileset

| Offset | Size | Type | Field |
|--------|------|------|-------|
| 0 | 64 | `char[]` | ID header string (null-padded) |
| 64 | 2 | `uint16` | Version (= 1) |
| 66 | 2 | `uint16` | Tile width in pixels (= 32) |
| 68 | 2 | `uint16` | Tile height in pixels (= 32) |
| 70 | 2 | `uint16` | Tile count |
| 72 | 768 | `uint8[768]` | Palette: 256 × (R, G, B) — each component 0–255 |
| 840 | 3 × tile_count | struct | Tile headers (see below) |
| 840 + 3×N | 1024 × tile_count | `uint8[]` | Pixel data: tile_count × 32 × 32 palette indices |

### Tile header (3 bytes each)

| Byte | Field |
|------|-------|
| 0 | Attribute flags |
| 1 | Move value (passability) |
| 2 | Average colour index |

Pixel data for tile *i* starts at offset `840 + 3 × tile_count + i × 1024` and is 1024 bytes of palette-indexed (8-bit) pixels in row-major order.

> **Note:** The editor uses an external Adobe Color Table (`.act`) palette for rendering rather than the palette embedded in the `.tls` file.  It searches parent directories of the `.tls` file for `netp.act` to match netPanzer's runtime appearance.
