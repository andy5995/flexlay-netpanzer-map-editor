#!/usr/bin/env python3
"""Append a 32×32 palette-indexed PNG as a new tile in a netPanzer .tls file.

The PNG must be 32×32 pixels in palette-indexed mode ('P') whose palette
colours match the tileset palette (netp.act or the palette embedded in the
.tls file).  In GIMP: Image → Mode → Indexed, pick the netPanzer palette,
then File → Export As … .png.

Usage:
    python3 tools/add_tile.py --tls path/to/summer12mb.tls --tile mytile.png

The new tile is appended at the end; its tile ID will be the old tile count.
A .bak backup of the original file is written before any changes are made.
"""

import argparse
import struct
import sys
from collections import Counter
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required:  pip install Pillow")

TLS_OFF_TILE_COUNT = 70
TLS_OFF_HEADERS    = 840   # 72-byte fixed header + 768-byte palette
TILE_PIXELS        = 32 * 32  # 1024 bytes of palette indices per tile


def load_tile_pixels(png_path: Path) -> bytes:
    img = Image.open(png_path)
    if img.size != (32, 32):
        sys.exit(f"Tile must be 32×32 pixels, got {img.size[0]}×{img.size[1]}")
    if img.mode != 'P':
        sys.exit(
            "Tile image must be palette-indexed (mode 'P').\n"
            "In GIMP: Image → Mode → Indexed, use the netPanzer palette, "
            "then export as PNG."
        )
    return bytes(img.tobytes())


def dominant_index(pixels: bytes) -> int:
    return Counter(pixels).most_common(1)[0][0]


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--tls",        required=True,
                    help="Path to the .tls file (modified in-place after backup)")
    ap.add_argument("--tile",       required=True,
                    help="32×32 palette-indexed PNG to append")
    ap.add_argument("--attrib",     type=int, default=0,
                    help="Tile attribute byte — 0=normal, 1=impassable (default 0)")
    ap.add_argument("--move-value", type=int, default=0,
                    help="Movement cost byte (default 0)")
    ap.add_argument("--dry-run",    action="store_true",
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
    px_offset  = TLS_OFF_HEADERS + tile_count * 3
    expected   = px_offset + tile_count * TILE_PIXELS

    if len(data) < expected:
        sys.exit(
            f"File too short: expected at least {expected} bytes for "
            f"{tile_count} tiles, got {len(data)}"
        )

    pixels  = load_tile_pixels(tile_path)
    avg_col = dominant_index(pixels)
    header  = bytes([args.attrib & 0xFF, args.move_value & 0xFF, avg_col])

    new_count = tile_count + 1
    # Insert the new tile header just after the existing headers block,
    # then append the pixel data at the end of the file.
    new_data = (
        data[:px_offset]   # fixed header + all existing tile headers
        + header            # new tile header
        + data[px_offset:] # existing pixel data (unchanged)
        + pixels            # new tile pixels
    )
    struct.pack_into("<H", new_data, TLS_OFF_TILE_COUNT, new_count)

    new_id = tile_count  # 0-based; was the old count
    print(f"Tileset    : {tls_path}")
    print(f"New tile   : {tile_path}")
    print(f"Tile count : {tile_count} → {new_count}  (new tile id = {new_id})")
    print(f"Header     : attrib={args.attrib}  move_value={args.move_value}"
          f"  avg_color={avg_col}")

    if args.dry_run:
        print("Dry run — nothing written.")
        return

    backup = tls_path.with_suffix(tls_path.suffix + ".bak")
    backup.write_bytes(data)
    print(f"Backup     : {backup}")

    tls_path.write_bytes(new_data)
    print("Done.")


if __name__ == "__main__":
    main()
