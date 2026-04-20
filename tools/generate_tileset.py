#!/usr/bin/env python3
"""
generate_tileset.py — Procedurally generate a new netPanzer-compatible tileset.

Produces:
  generated.tls           — binary .tls tileset (netPanzer format)
  generated.act           — 768-byte RGB palette file (overrides netp.act lookup)
  generated.autotile.json — autotile config for the Qt editor

Terrain types and their blob-8 autotile bitmask sets (47 canonical blobs each):
  grass (0), mountain (1), water (2), sand (3), road (4)

Tile layout: terrain_index * 47 + bitmask_rank → tile_id
Total tiles: 5 * 47 = 235

Usage:
  python3 tools/generate_tileset.py [--out-dir PATH]
"""

import argparse
import json
import math
import os
import random
import struct

# ---------------------------------------------------------------------------
# Blob-8 canonical bitmasks
# ---------------------------------------------------------------------------

def canonical_bitmasks():
    """Return all 47 valid blob-8 bitmasks (diagonal only counts if both cardinals set)."""
    blobs = set()
    for raw in range(256):
        n  = bool(raw & 1)
        ne = bool(raw & 2)
        e  = bool(raw & 4)
        se = bool(raw & 8)
        s  = bool(raw & 16)
        sw = bool(raw & 32)
        w  = bool(raw & 64)
        nw = bool(raw & 128)
        # Clamp diagonals
        ne = ne and n and e
        se = se and s and e
        sw = sw and s and w
        nw = nw and n and w
        canon = (n*1 | ne*2 | e*4 | se*8 | s*16 | sw*32 | w*64 | nw*128)
        blobs.add(canon)
    return sorted(blobs)

BITMASKS = canonical_bitmasks()  # 47 entries, sorted
assert len(BITMASKS) == 47

BM_RANK = {bm: i for i, bm in enumerate(BITMASKS)}  # bitmask → index within terrain block

# ---------------------------------------------------------------------------
# Palette (256 colours)
# ---------------------------------------------------------------------------

# Base colours per terrain + edge/transition slots
# Index layout:
#   0-7    grass shades
#   8-15   mountain shades
#   16-23  water shades
#   24-31  sand shades
#   32-39  road shades
#   40-47  shared highlights / transition edge tones
#   48-63  noise grain colours (dark variants)
#   64-255 spare (black)

TERRAIN_BASE = {
    'grass':    0x4a7c3f,
    'mountain': 0x7a6a52,
    'water':    0x2a5f8f,
    'sand':     0xc8b46a,
    'road':     0x5c5c5c,
}
TERRAIN_ORDER = ['grass', 'mountain', 'water', 'sand', 'road']

def hex_to_rgb(h):
    return ((h >> 16) & 0xff, (h >> 8) & 0xff, h & 0xff)

def lerp_color(c1, c2, t):
    r = int(c1[0] + (c2[0]-c1[0]) * t)
    g = int(c1[1] + (c2[1]-c1[1]) * t)
    b = int(c1[2] + (c2[2]-c1[2]) * t)
    return (r, g, b)

def build_palette():
    pal = [(0, 0, 0)] * 256

    # 8 shades per terrain (palette indices 0-39)
    for ti, name in enumerate(TERRAIN_ORDER):
        base = hex_to_rgb(TERRAIN_BASE[name])
        for shade in range(8):
            t = shade / 7.0
            dark = (max(0, base[0]-60), max(0, base[1]-60), max(0, base[2]-60))
            light = (min(255, base[0]+60), min(255, base[1]+60), min(255, base[2]+60))
            pal[ti*8 + shade] = lerp_color(dark, light, t)

    # Shared neutral tones (40-47)
    for i in range(8):
        v = 80 + i * 20
        pal[40 + i] = (v, v, v)

    # Noise grain variants (48-63) — darkened terrain bases
    for ti, name in enumerate(TERRAIN_ORDER):
        base = hex_to_rgb(TERRAIN_BASE[name])
        for g in range(3):
            dark = (max(0, base[0]-30-g*15),
                    max(0, base[1]-30-g*15),
                    max(0, base[2]-30-g*15))
            pal[48 + ti*3 + g] = dark

    return pal

# ---------------------------------------------------------------------------
# Tile pixel generation
# ---------------------------------------------------------------------------

TILE_W = 32
TILE_H = 32

rng = random.Random(42)

def terrain_palette_base(ti):
    """Middle shade index for terrain ti (index 3 of 8 shades = bright-ish)."""
    return ti * 8 + 4

def noise_idx(ti, strength):
    """Return a palette index for a noise pixel given terrain and strength 0-2."""
    return 48 + ti * 3 + min(2, strength)

def pixel_for(terrain_idx, nx, ny):
    """Return palette index for a pixel at (nx,ny) in [0,1] with noise grain."""
    base = terrain_palette_base(terrain_idx)
    # Simple hash noise
    h = int(abs(math.sin(nx * 127.1 + ny * 311.7) * 43758.5453)) % 16
    if h < 3:
        return noise_idx(terrain_idx, h % 3)
    shade_offset = (h % 8) - 4  # -4..3
    idx = terrain_palette_base(terrain_idx) + shade_offset
    idx = max(terrain_idx * 8, min(terrain_idx * 8 + 7, idx))
    return idx

def make_tile_pixels(terrain_idx, bitmask):
    """
    Generate 32×32 raw palette-indexed pixels for a terrain+bitmask combo.

    The tile shows the terrain fill; edges that face a 'missing' neighbour
    get a subtle darkened border to hint at the transition.
    """
    pixels = bytearray(TILE_W * TILE_H)

    n  = bool(bitmask & 1)
    e  = bool(bitmask & 4)
    s  = bool(bitmask & 16)
    w  = bool(bitmask & 64)
    ne = bool(bitmask & 2)
    se = bool(bitmask & 8)
    sw = bool(bitmask & 32)
    nw = bool(bitmask & 128)

    BORDER = 4  # px border width for edge hint

    for py in range(TILE_H):
        for px in range(TILE_W):
            nx = px / (TILE_W - 1)
            ny = py / (TILE_H - 1)

            # Check if this pixel is in an exposed edge zone
            on_n = py < BORDER
            on_s = py >= TILE_H - BORDER
            on_e = px >= TILE_W - BORDER
            on_w = px < BORDER

            # Darken edge pixels that face a non-member neighbour
            edge_dark = False
            if on_n and not n: edge_dark = True
            if on_s and not s: edge_dark = True
            if on_e and not e: edge_dark = True
            if on_w and not w: edge_dark = True
            # Corner pixels: darken if either diagonal or both cardinals missing
            if on_n and on_e and not ne: edge_dark = True
            if on_s and on_e and not se: edge_dark = True
            if on_s and on_w and not sw: edge_dark = True
            if on_n and on_w and not nw: edge_dark = True

            if edge_dark:
                idx = terrain_idx * 8 + 0  # darkest shade
            else:
                idx = pixel_for(terrain_idx, nx + terrain_idx * 0.13, ny + terrain_idx * 0.07)

            pixels[py * TILE_W + px] = idx

    return pixels

# ---------------------------------------------------------------------------
# .tls binary layout (matches npmformat.h / tlsloader.cpp)
# ---------------------------------------------------------------------------

# Offsets (from npmformat.h):
#   0   64-byte ID string
#   64  uint16 version (=1)
#   66  uint16 x_pix  (=32)
#   68  uint16 y_pix  (=32)
#   70  uint16 tile_count
#   72  768-byte palette (256 × RGB)
#   840 tile_count × 3 TILE_HEADERs (attrib, move_value, avg_color)
#   840+tile_count*3  raw pixels (tile_count × 32 × 32)

ID_STRING = b'Surface Tileset\x00' + b'\x00' * 48  # pad to 64 bytes
ID_STRING = (b'Surface Tileset\x00').ljust(64, b'\x00')

MOVE_VALUES = {
    'grass':    0,   # passable
    'mountain': 3,   # impassable
    'water':    4,   # water
    'sand':     1,   # slower
    'road':     0,   # fast
}

def write_tls(path, pal, all_pixels):
    """Write a valid .tls file."""
    tile_count = len(all_pixels)
    with open(path, 'wb') as f:
        f.write(ID_STRING)
        f.write(struct.pack('<HHHH', 1, TILE_W, TILE_H, tile_count))
        # 768-byte palette
        for r, g, b in pal:
            f.write(bytes([r, g, b]))
        # Tile headers (attrib=0, move_value, avg_color=0)
        for ti_name, bm in tile_order():
            ti = TERRAIN_ORDER.index(ti_name)
            mv = MOVE_VALUES[ti_name]
            f.write(bytes([0, mv, ti * 8 + 4]))  # attrib, move_value, avg_color
        # Raw pixels
        for pix in all_pixels:
            f.write(pix)

def tile_order():
    """Yield (terrain_name, bitmask) pairs in the order tiles are written."""
    for name in TERRAIN_ORDER:
        for bm in BITMASKS:
            yield (name, bm)

# ---------------------------------------------------------------------------
# .act palette file
# ---------------------------------------------------------------------------

def write_act(path, pal):
    with open(path, 'wb') as f:
        for r, g, b in pal:
            f.write(bytes([r, g, b]))

# ---------------------------------------------------------------------------
# autotile JSON
# ---------------------------------------------------------------------------

def write_autotile_json(path):
    groups = []

    tile_base = 0
    for name in TERRAIN_ORDER:
        bitmask_map = {}
        for i, bm in enumerate(BITMASKS):
            bitmask_map[str(bm)] = tile_base + i
        member_tiles = list(range(tile_base, tile_base + len(BITMASKS)))
        groups.append({
            "name": name,
            "type": "blob8",
            "member_tiles": member_tiles,
            "bitmask": bitmask_map,
        })
        tile_base += len(BITMASKS)

    with open(path, 'w') as f:
        json.dump({"groups": groups}, f, indent=2)
    print(f"  wrote {path}")

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out-dir', default='.', help='Output directory')
    args = ap.parse_args()

    out = args.out_dir
    os.makedirs(out, exist_ok=True)

    print("Building palette...")
    pal = build_palette()

    print(f"Generating {len(TERRAIN_ORDER) * len(BITMASKS)} tiles ({len(TERRAIN_ORDER)} terrains × {len(BITMASKS)} bitmasks)...")
    all_pixels = []
    for name in TERRAIN_ORDER:
        ti = TERRAIN_ORDER.index(name)
        for bm in BITMASKS:
            all_pixels.append(make_tile_pixels(ti, bm))
        print(f"  {name}: {len(BITMASKS)} tiles")

    # Mirror the summer12mb layout: wads/generated/Default/generated.tls
    wads_dir = os.path.join(out, 'Default')
    os.makedirs(wads_dir, exist_ok=True)

    tls_path  = os.path.join(wads_dir, 'generated.tls')
    act_path  = os.path.join(wads_dir, 'generated.act')
    cfg_path  = os.path.join(wads_dir, 'Default.cfg')
    json_path = os.path.join(out, 'generated.autotile.json')

    print(f"Writing {tls_path} ...")
    write_tls(tls_path, pal, all_pixels)

    print(f"Writing {act_path} ...")
    write_act(act_path, pal)

    with open(cfg_path, 'w') as f:
        f.write("craters_lifetime = 0\ncraters_fading = 0\nunits_shadow_blending = 0\n")
    print(f"  wrote {cfg_path}")

    write_autotile_json(json_path)

    tile_count = len(all_pixels)
    tls_size = os.path.getsize(tls_path)
    print(f"\nDone.")
    print(f"  Tiles: {tile_count}")
    print(f"  .tls size: {tls_size} bytes")
    print(f"\nTo use in the editor:")
    print(f"  Place the output dir as data/wads/generated/")
    print(f"  Copy {json_path} to data/autotile/")
    print(f"  In the New Map dialog, type 'generated.tls' or use Browse.")

if __name__ == '__main__':
    main()
