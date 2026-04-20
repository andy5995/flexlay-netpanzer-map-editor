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
Total tiles: 5 * 47 = 235, followed by any embedded groups.

Embedding tiles from an existing tileset
-----------------------------------------
Use --source-tls to pull tiles from another .tls file (e.g. summer12mb) and
append them as a named group. The source palette is merged into the 192 spare
slots (64-255) of the generated palette using nearest-colour remapping.

  python3 tools/generate_tileset.py --out-dir data/wads/generated \\
      --source-tls ~/src/netpanzer/data/wads/summer12mb/SummerDay/summer12mb.tls \\
      --source-act ~/src/netpanzer/data/wads/netp.act \\
      --embed 9541 10117 outpost

  --embed START END NAME   tile IDs [START, END) from the source TLS
                           (may be repeated for multiple groups)

Usage:
  python3 tools/generate_tileset.py [--out-dir PATH] [--source-tls PATH]
      [--source-act PATH] [--embed START END NAME ...]
"""

import argparse
import json
import math
import os
import struct
from collections import Counter

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
        ne = ne and n and e
        se = se and s and e
        sw = sw and s and w
        nw = nw and n and w
        canon = (n*1 | ne*2 | e*4 | se*8 | s*16 | sw*32 | w*64 | nw*128)
        blobs.add(canon)
    return sorted(blobs)

BITMASKS = canonical_bitmasks()  # 47 entries, sorted
assert len(BITMASKS) == 47

# ---------------------------------------------------------------------------
# Palette (256 colours)
# ---------------------------------------------------------------------------

# Index layout:
#   0-7    grass shades
#   8-15   mountain shades
#   16-23  water shades
#   24-31  sand shades
#   32-39  road shades
#   40-47  shared highlights / transition tones
#   48-63  noise grain colours (dark terrain variants)
#   64-255 spare — filled with source palette colours when --embed is used

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

    for ti, name in enumerate(TERRAIN_ORDER):
        base = hex_to_rgb(TERRAIN_BASE[name])
        for shade in range(8):
            t = shade / 7.0
            dark  = (max(0, base[0]-60), max(0, base[1]-60), max(0, base[2]-60))
            light = (min(255, base[0]+60), min(255, base[1]+60), min(255, base[2]+60))
            pal[ti*8 + shade] = lerp_color(dark, light, t)

    for i in range(8):
        v = 80 + i * 20
        pal[40 + i] = (v, v, v)

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
BORDER = 16  # half the tile — makes transitions clearly visible

TERRAIN_NEIGHBOR = {
    'grass':    None,
    'mountain': 'grass',
    'water':    'sand',
    'sand':     'grass',
    'road':     'grass',
}

def terrain_palette_base(ti):
    return ti * 8 + 4

def noise_idx(ti, strength):
    return 48 + ti * 3 + min(2, strength)

def pixel_for(terrain_idx, nx, ny):
    h = int(abs(math.sin(nx * 127.1 + ny * 311.7) * 43758.5453)) % 16
    if h < 3:
        return noise_idx(terrain_idx, h % 3)
    shade_offset = (h % 8) - 4
    idx = terrain_palette_base(terrain_idx) + shade_offset
    return max(terrain_idx * 8, min(terrain_idx * 8 + 7, idx))

def edge_dist(px, py, n, e, s, w, ne, se, sw, nw):
    INF = BORDER + 1
    d = INF
    if not n:  d = min(d, py)
    if not s:  d = min(d, TILE_H - 1 - py)
    if not e:  d = min(d, TILE_W - 1 - px)
    if not w:  d = min(d, px)
    if n and e and not ne: d = min(d, max(TILE_W - 1 - px, py))
    if s and e and not se: d = min(d, max(TILE_W - 1 - px, TILE_H - 1 - py))
    if s and w and not sw: d = min(d, max(px, TILE_H - 1 - py))
    if n and w and not nw: d = min(d, max(px, py))
    return d

def make_tile_pixels(terrain_idx, bitmask, neighbor_idx):
    """
    Generate 32×32 palette-indexed pixels.
    Exposed edges (within BORDER px) are painted as the neighbor terrain.
    """
    pixels = bytearray(TILE_W * TILE_H)

    n  = bool(bitmask & 1);  e  = bool(bitmask & 4)
    s  = bool(bitmask & 16); w  = bool(bitmask & 64)
    ne = bool(bitmask & 2);  se = bool(bitmask & 8)
    sw = bool(bitmask & 32); nw = bool(bitmask & 128)

    for py in range(TILE_H):
        for px in range(TILE_W):
            fnx = px / (TILE_W - 1)
            fny = py / (TILE_H - 1)
            dist = edge_dist(px, py, n, e, s, w, ne, se, sw, nw)
            if dist < BORDER:
                if neighbor_idx is not None:
                    idx = pixel_for(neighbor_idx,
                                    fnx + neighbor_idx * 0.13,
                                    fny + neighbor_idx * 0.07)
                else:
                    idx = terrain_idx * 8 + 1
            else:
                idx = pixel_for(terrain_idx,
                                fnx + terrain_idx * 0.13,
                                fny + terrain_idx * 0.07)
            pixels[py * TILE_W + px] = idx

    return pixels

# ---------------------------------------------------------------------------
# Source TLS embedding
# ---------------------------------------------------------------------------

def load_source_tls(tls_path, act_path=None):
    """Load raw tile bytes and palette from a .tls file."""
    with open(tls_path, 'rb') as f:
        data = f.read()
    _ver, xpix, ypix, tile_count = struct.unpack_from('<HHHH', data, 64)
    assert xpix == 32 and ypix == 32

    src_pal = [(data[72 + i*3], data[72 + i*3 + 1], data[72 + i*3 + 2])
               for i in range(256)]

    if act_path and os.path.exists(act_path):
        with open(act_path, 'rb') as f:
            act = f.read()
        if len(act) >= 768:
            src_pal = [(act[i*3], act[i*3+1], act[i*3+2]) for i in range(256)]

    px_offset = 840 + tile_count * 3
    stride = xpix * ypix
    actual = min(tile_count, (len(data) - px_offset) // stride)

    raw_tiles = [data[px_offset + i*stride : px_offset + (i+1)*stride]
                 for i in range(actual)]
    return raw_tiles, src_pal


def merge_palette(pal, src_pal, src_tiles):
    """
    Find the most-used colours in src_tiles (via src_pal) and add them to pal
    in slots 64-255.  Returns a remap table: src_pal_index → pal_index.
    """
    # Count how often each src palette index is used
    counter = Counter()
    for tile in src_tiles:
        for b in tile:
            counter[b] += 1

    # Build ordered list: src indices by frequency, most common first
    ordered = [idx for idx, _ in counter.most_common()]

    # Fill spare palette slots 64-255 with the corresponding src colours.
    # If a colour is already close to an existing slot, reuse it.
    FREE_START = 64
    slot = FREE_START

    src_to_dst = {}  # src palette index → generated palette index

    for src_idx in ordered:
        if src_idx in src_to_dst:
            continue
        color = src_pal[src_idx]

        # Check if this colour already exists in pal (slots 0-slot-1)
        best_existing = None
        best_dist = float('inf')
        for i in range(slot):
            pr, pg, pb = pal[i]
            cr, cg, cb = color
            d = (pr-cr)**2 + (pg-cg)**2 + (pb-cb)**2
            if d < best_dist:
                best_dist = d
                best_existing = i

        if best_dist < 100:  # close enough — reuse
            src_to_dst[src_idx] = best_existing
            continue

        if slot >= 256:
            # Palette full — map to nearest existing colour
            src_to_dst[src_idx] = best_existing
            continue

        pal[slot] = color
        src_to_dst[src_idx] = slot
        slot += 1

    # Any unmapped src indices → nearest colour in pal
    for src_idx in range(256):
        if src_idx not in src_to_dst:
            color = src_pal[src_idx]
            best, best_d = 0, float('inf')
            for i in range(256):
                pr, pg, pb = pal[i]
                cr, cg, cb = color
                d = (pr-cr)**2 + (pg-cg)**2 + (pb-cb)**2
                if d < best_d:
                    best_d = d
                    best = i
            src_to_dst[src_idx] = best

    remap = [src_to_dst[i] for i in range(256)]
    print(f"    palette slots used: {slot - FREE_START} / 192")
    return remap


def remap_tiles(raw_tiles, remap):
    return [bytes(remap[b] for b in tile) for tile in raw_tiles]

# ---------------------------------------------------------------------------
# .tls writer
# ---------------------------------------------------------------------------

ID_STRING = (b'Surface Tileset\x00').ljust(64, b'\x00')

MOVE_VALUES = {
    'grass':    0,
    'mountain': 3,
    'water':    4,
    'sand':     1,
    'road':     0,
}

def write_tls(path, pal, all_pixels, headers):
    """
    headers: list of (attrib, move_value, avg_color) per tile, same length as all_pixels.
    """
    tile_count = len(all_pixels)
    with open(path, 'wb') as f:
        f.write(ID_STRING)
        f.write(struct.pack('<HHHH', 1, TILE_W, TILE_H, tile_count))
        for r, g, b in pal:
            f.write(bytes([r, g, b]))
        for attrib, mv, avg in headers:
            f.write(bytes([attrib, mv, avg]))
        for pix in all_pixels:
            f.write(pix)

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

def write_autotile_json(path, terrain_tile_base, embedded_groups):
    groups = []

    tile_base = terrain_tile_base
    for name in TERRAIN_ORDER:
        bitmask_map = {str(bm): tile_base + i for i, bm in enumerate(BITMASKS)}
        member_tiles = list(range(tile_base, tile_base + len(BITMASKS)))
        groups.append({
            "name": name,
            "type": "blob8",
            "member_tiles": member_tiles,
            "bitmask": bitmask_map,
        })
        tile_base += len(BITMASKS)

    for name, start, count in embedded_groups:
        groups.append({
            "name": name,
            "member_tiles": list(range(start, start + count)),
        })

    with open(path, 'w') as f:
        json.dump({"groups": groups}, f, indent=2)
    print(f"  wrote {path}")

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out-dir', default='.', help='Output directory')
    ap.add_argument('--source-tls', default=None,
                    help='Source .tls to embed tiles from')
    ap.add_argument('--source-act', default=None,
                    help='Palette for source .tls (default: netp.act beside it)')
    ap.add_argument('--embed', nargs=3, action='append',
                    metavar=('START', 'END', 'NAME'),
                    help='Embed tile range [START,END) from source as group NAME')
    args = ap.parse_args()

    out = args.out_dir
    os.makedirs(out, exist_ok=True)

    print("Building palette...")
    pal = build_palette()

    # -- Terrain tiles --------------------------------------------------------
    print(f"Generating terrain tiles "
          f"({len(TERRAIN_ORDER)} terrains × {len(BITMASKS)} bitmasks)...")
    all_pixels = []
    headers    = []

    for name in TERRAIN_ORDER:
        ti     = TERRAIN_ORDER.index(name)
        nb_name = TERRAIN_NEIGHBOR[name]
        nb_idx  = TERRAIN_ORDER.index(nb_name) if nb_name else None
        mv      = MOVE_VALUES[name]
        avg     = ti * 8 + 4
        for bm in BITMASKS:
            all_pixels.append(make_tile_pixels(ti, bm, nb_idx))
            headers.append((0, mv, avg))
        print(f"  {name}: {len(BITMASKS)} tiles")

    # -- Embedded groups ------------------------------------------------------
    embedded_groups = []  # (name, start_tile_id, count)

    if args.embed:
        if not args.source_tls:
            ap.error("--embed requires --source-tls")

        act_path = args.source_act
        if act_path is None:
            # Default: netp.act beside the source .tls
            act_path = os.path.join(os.path.dirname(args.source_tls), '..', '..', 'netp.act')
            act_path = os.path.normpath(act_path)

        print(f"\nLoading source tileset: {args.source_tls}")
        src_tiles, src_pal = load_source_tls(args.source_tls, act_path)
        print(f"  {len(src_tiles)} source tiles loaded")

        for start_s, end_s, grp_name in args.embed:
            start = int(start_s)
            end   = int(end_s)
            subset = src_tiles[start:min(end, len(src_tiles))]
            print(f"\nEmbedding '{grp_name}' tiles {start}-{end-1} ({len(subset)} tiles)...")
            remap = merge_palette(pal, src_pal, subset)
            remapped = remap_tiles(subset, remap)

            tile_start = len(all_pixels)
            all_pixels.extend(remapped)
            # Embedded tiles: attrib=0, move_value=3 (impassable), avg_color=40 (neutral)
            headers.extend([(0, 3, 40)] * len(remapped))
            embedded_groups.append((grp_name, tile_start, len(remapped)))
            print(f"  added {len(remapped)} tiles (IDs {tile_start}–{tile_start+len(remapped)-1})")

    # -- Write files ----------------------------------------------------------
    wads_dir = os.path.join(out, 'Default')
    os.makedirs(wads_dir, exist_ok=True)

    tls_path  = os.path.join(wads_dir, 'generated.tls')
    act_path  = os.path.join(wads_dir, 'generated.act')
    cfg_path  = os.path.join(wads_dir, 'Default.cfg')
    json_path = os.path.join(out, 'generated.autotile.json')

    print(f"\nWriting {tls_path} ...")
    write_tls(tls_path, pal, all_pixels, headers)

    print(f"Writing {act_path} ...")
    write_act(act_path, pal)

    with open(cfg_path, 'w') as f:
        f.write("craters_lifetime = 0\ncraters_fading = 0\nunits_shadow_blending = 0\n")

    write_autotile_json(json_path, 0, embedded_groups)

    print(f"\nDone.  Tiles: {len(all_pixels)}  "
          f".tls size: {os.path.getsize(tls_path)} bytes")
    print(f"\nTo use in the editor:")
    print(f"  Place the output dir as data/wads/generated/")
    print(f"  Copy {json_path} to data/autotile/")
    print(f"  In the New Map dialog, type 'generated.tls' or use Browse.")

if __name__ == '__main__':
    main()
