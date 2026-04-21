#!/usr/bin/env python3
"""Replace an existing tile in a netPanzer .tls file with a new 32×32 PNG.

The PNG must be 32×32 pixels.  It may be RGB, RGBA, or palette-indexed (mode P).
The script quantizes the image to the palette embedded in the .tls file, so no
manual indexed-mode conversion is needed — exporting as plain RGB from GIMP works.

Usage:
    python3 tools/replace_tile.py --tls path/to/summer12mb.tls \\
        --id 9467 --tile newtile.png

A .bak backup of the original file is written before any changes are made.
"""

import argparse
import struct
import sys
from collections import Counter
from pathlib import Path

try:
    from PIL import Image
    import numpy as np
except ImportError:
    sys.exit("Pillow and numpy are required:  pip install Pillow numpy")

TLS_OFF_TILE_COUNT = 70
TLS_OFF_HEADERS    = 840   # 72-byte fixed header + 768-byte palette
TLS_OFF_PALETTE    = 72    # 768 bytes = 256 × 3 RGB
TILE_PIXELS        = 32 * 32


TERRAIN_PALETTE_END = 85  # indices 0-84 are terrain; 85+ are UI/special colours


def quantize_to_tileset_palette(img: Image.Image, tls_data: bytes) -> bytes:
    """Quantize to the terrain portion of the tileset palette (indices 0–84)."""
    full_pal = np.frombuffer(
        tls_data[TLS_OFF_PALETTE : TLS_OFF_PALETTE + 768], dtype=np.uint8
    ).reshape(256, 3).astype(np.int32)

    terrain_pal = full_pal[:TERRAIN_PALETTE_END]

    pixels = np.array(img.convert("RGB"), dtype=np.int32).reshape(-1, 3)

    diff = pixels[:, np.newaxis, :] - terrain_pal[np.newaxis, :, :]
    indices = (diff * diff).sum(axis=2).argmin(axis=1).astype(np.uint8)

    return bytes(indices.tobytes())


def load_tile_pixels(png_path: Path, tls_data: bytes) -> bytes:
    img = Image.open(png_path)
    if img.size != (32, 32):
        sys.exit(f"Tile must be 32×32 pixels, got {img.size[0]}×{img.size[1]}")
    return quantize_to_tileset_palette(img, tls_data)


def dominant_index(pixels: bytes) -> int:
    return Counter(pixels).most_common(1)[0][0]


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--tls",      required=True,
                    help="Path to the .tls file (modified in-place after backup)")
    ap.add_argument("--id",       required=True, type=int,
                    help="0-based tile ID to replace")
    ap.add_argument("--tile",     required=True,
                    help="32×32 palette-indexed PNG to write into the slot")
    ap.add_argument("--attrib",   type=int, default=None,
                    help="Override tile attribute byte (0=normal, 1=impassable); "
                         "default: keep existing")
    ap.add_argument("--move-value", type=int, default=None,
                    help="Override movement cost byte; default: keep existing")
    ap.add_argument("--dry-run",  action="store_true",
                    help="Print what would happen without writing anything")
    args = ap.parse_args()

    tls_path  = Path(args.tls)
    tile_path = Path(args.tile)

    if not tls_path.exists():
        sys.exit(f"Not found: {tls_path}")
    if not tile_path.exists():
        sys.exit(f"Not found: {tile_path}")

    data = bytearray(tls_path.read_bytes())

    tile_count = struct.unpack_from("<H", data, TLS_OFF_TILE_COUNT)[0]
    tile_id    = args.id

    if tile_id < 0 or tile_id >= tile_count:
        sys.exit(f"Tile ID {tile_id} out of range (tileset has {tile_count} tiles, IDs 0–{tile_count-1})")

    # Header for tile N is at: TLS_OFF_HEADERS + N*3
    hdr_offset = TLS_OFF_HEADERS + tile_id * 3
    # Pixel data starts after ALL headers
    px_base    = TLS_OFF_HEADERS + tile_count * 3
    px_offset  = px_base + tile_id * TILE_PIXELS

    old_attrib     = data[hdr_offset]
    old_move_value = data[hdr_offset + 1]
    old_avg_color  = data[hdr_offset + 2]

    pixels    = load_tile_pixels(tile_path, data)
    avg_color = dominant_index(pixels)

    new_attrib     = args.attrib     if args.attrib     is not None else old_attrib
    new_move_value = args.move_value if args.move_value is not None else old_move_value

    print(f"Tileset    : {tls_path}")
    print(f"Tile ID    : {tile_id}  (of {tile_count})")
    print(f"New tile   : {tile_path}")
    print(f"Header was : attrib={old_attrib}  move_value={old_move_value}  avg_color={old_avg_color}")
    print(f"Header new : attrib={new_attrib}  move_value={new_move_value}  avg_color={avg_color}")

    if args.dry_run:
        print("Dry run — nothing written.")
        return

    backup = tls_path.with_suffix(tls_path.suffix + ".bak")
    backup.write_bytes(data)
    print(f"Backup     : {backup}")

    # Write updated header
    data[hdr_offset]     = new_attrib     & 0xFF
    data[hdr_offset + 1] = new_move_value & 0xFF
    data[hdr_offset + 2] = avg_color      & 0xFF

    # Write pixel data
    data[px_offset : px_offset + TILE_PIXELS] = pixels

    tls_path.write_bytes(data)
    print("Done.")


if __name__ == "__main__":
    main()
