#!/usr/bin/env python3
"""Append a 32×32 PNG as a new tile in a netPanzer .tls file.

The PNG must be 32×32 pixels.  It may be RGB, RGBA, or palette-indexed (mode P).
The script quantizes the image to the palette embedded in the .tls file, so no
manual indexed-mode conversion is needed — exporting as plain RGB from GIMP works.

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
    import numpy as np
except ImportError:
    sys.exit("Pillow and numpy are required:  pip install Pillow numpy")

TLS_OFF_TILE_COUNT = 70
TLS_OFF_HEADERS    = 840   # 72-byte fixed header + 768-byte palette
TLS_OFF_PALETTE    = 72    # 768 bytes = 256 × 3 RGB
TILE_PIXELS        = 32 * 32  # 1024 bytes of palette indices per tile


def _load_act_palette(tls_path: Path) -> np.ndarray:
    """Load netp.act by walking up from the .tls file, mirroring the editor's logic."""
    import os
    d = tls_path.parent
    for _ in range(6):
        candidate = d / "netp.act"
        if candidate.exists():
            data = candidate.read_bytes()
            return np.frombuffer(data[:768], dtype=np.uint8).reshape(256, 3)
        d = d.parent
    return None


def quantize_to_tileset_palette(img: Image.Image, tls_data: bytes,
                                 tls_path: Path = None,
                                 neighbor_ids: list = None) -> bytes:
    """Quantize using netp.act (the palette the editor uses).

    If neighbor_ids is given, restricts to palette indices actually used by
    those tiles so the result blends naturally with surrounding tiles.
    """
    act_pal = None
    if tls_path is not None:
        act_pal = _load_act_palette(tls_path)

    if act_pal is None:
        # Fall back to TLS embedded palette
        act_pal = np.frombuffer(
            tls_data[TLS_OFF_PALETTE : TLS_OFF_PALETTE + 768], dtype=np.uint8
        ).reshape(256, 3)

    if neighbor_ids:
        count = struct.unpack_from("<H", tls_data, TLS_OFF_TILE_COUNT)[0]
        px_base = TLS_OFF_HEADERS + count * 3
        allowed = set()
        for tid in neighbor_ids:
            if 0 <= tid < count:
                raw = tls_data[px_base + tid * TILE_PIXELS : px_base + (tid + 1) * TILE_PIXELS]
                allowed.update(raw)
        allowed_list = sorted(allowed)
    else:
        allowed_list = list(range(256))

    sub_pal = act_pal[allowed_list].astype(np.int32)  # (N, 3)
    pixels   = np.array(img.convert("RGB"), dtype=np.int32).reshape(-1, 3)

    diff    = pixels[:, np.newaxis, :] - sub_pal[np.newaxis, :, :]
    nearest = (diff * diff).sum(axis=2).argmin(axis=1)
    indices = np.array(allowed_list, dtype=np.uint8)[nearest]

    return bytes(indices.tobytes())


def load_tile_pixels(png_path: Path, tls_data: bytes, tls_path: Path = None,
                     neighbor_ids: list = None) -> bytes:
    img = Image.open(png_path)
    if img.size != (32, 32):
        sys.exit(f"Tile must be 32×32 pixels, got {img.size[0]}×{img.size[1]}")
    return quantize_to_tileset_palette(img, tls_data, tls_path, neighbor_ids)


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
                    help="32×32 PNG to append (RGB, RGBA, or indexed)")
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

    pixels  = load_tile_pixels(tile_path, data, tls_path)
    avg_col = dominant_index(pixels)
    header  = bytes([args.attrib & 0xFF, args.move_value & 0xFF, avg_col])

    new_count = tile_count + 1
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
