#!/usr/bin/env python3
"""
derive_autotile_bitmasks.py — Infer blob-8 bitmask mappings for summer12mb.tls
by analysing tile pixel data.

Strategy
--------
For each terrain group defined in the input autotile JSON:
  1. Find the "interior" reference tile — the tile whose edges most closely
     resemble its own centre (i.e. the fully-surrounded, bitmask-255 tile).
  2. For every other tile in the group, compare each of the 8 directional
     edge zones against the reference tile's same zone.
  3. A zone that matches the reference is treated as "neighbour present"
     (bit = 1); one that diverges is "exposed edge" (bit = 0).
  4. Build the bitmask and select the best tile per canonical bitmask value.

Output
------
  <out>.autotile.json   — copy of the input with "bitmask" populated for
                          every group that was analysable.
  <out>.bitmask_report.txt — per-group confidence scores for review.

Usage
-----
  python3 tools/derive_autotile_bitmasks.py \\
      --tls  /path/to/summer12mb.tls \\
      --act  /path/to/netp.act \\
      --json data/autotile/summer12mb.autotile.json \\
      --out  /tmp/summer12mb_derived
"""

import argparse
import json
import math
import os
import struct

# ---------------------------------------------------------------------------
# TLS loader
# ---------------------------------------------------------------------------

def load_tls(tls_path, act_path=None):
    with open(tls_path, 'rb') as f:
        data = f.read()

    version, xpix, ypix, tile_count = struct.unpack_from('<HHHH', data, 64)
    assert xpix == 32 and ypix == 32, "Only 32×32 tiles supported"

    # Palette from TLS
    pal = []
    for i in range(256):
        base = 72 + i * 3
        pal.append((data[base], data[base+1], data[base+2]))

    # Override with netp.act if provided
    if act_path and os.path.exists(act_path):
        with open(act_path, 'rb') as f:
            act = f.read()
        if len(act) >= 768:
            pal = [(act[i*3], act[i*3+1], act[i*3+2]) for i in range(256)]

    px_offset = 840 + tile_count * 3
    stride = xpix * ypix
    actual = min(tile_count, (len(data) - px_offset) // stride)

    tiles = []
    for i in range(actual):
        start = px_offset + i * stride
        tiles.append(data[start:start+stride])

    return tiles, pal, xpix, ypix


# ---------------------------------------------------------------------------
# Edge zone extraction
# ---------------------------------------------------------------------------

W = H = 32
ZONE_W = 5   # how many pixels wide each directional zone is

def tile_rgb(tile_bytes, pal):
    """Convert indexed tile to flat list of (R,G,B) tuples."""
    return [pal[b] for b in tile_bytes]

def zone_pixels(rgb, direction):
    """
    Return pixel list for one of the 8 directional zones.
    Zones are ZONE_W pixels wide/tall; diagonal zones are ZONE_W×ZONE_W corners.
    """
    z = ZONE_W
    pixels = []
    if direction == 'N':
        for y in range(z):
            for x in range(W):
                pixels.append(rgb[y * W + x])
    elif direction == 'S':
        for y in range(H - z, H):
            for x in range(W):
                pixels.append(rgb[y * W + x])
    elif direction == 'E':
        for y in range(H):
            for x in range(W - z, W):
                pixels.append(rgb[y * W + x])
    elif direction == 'W':
        for y in range(H):
            for x in range(z):
                pixels.append(rgb[y * W + x])
    elif direction == 'NE':
        for y in range(z):
            for x in range(W - z, W):
                pixels.append(rgb[y * W + x])
    elif direction == 'SE':
        for y in range(H - z, H):
            for x in range(W - z, W):
                pixels.append(rgb[y * W + x])
    elif direction == 'SW':
        for y in range(H - z, H):
            for x in range(z):
                pixels.append(rgb[y * W + x])
    elif direction == 'NW':
        for y in range(z):
            for x in range(z):
                pixels.append(rgb[y * W + x])
    return pixels

DIRECTIONS = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW']
BIT = {'N': 1, 'NE': 2, 'E': 4, 'SE': 8, 'S': 16, 'SW': 32, 'W': 64, 'NW': 128}

def zone_distance(z1, z2):
    """Mean Euclidean RGB distance between two equal-length pixel lists."""
    total = 0.0
    for (r1,g1,b1), (r2,g2,b2) in zip(z1, z2):
        total += math.sqrt((r1-r2)**2 + (g1-g2)**2 + (b1-b2)**2)
    return total / len(z1) if z1 else 0.0


# ---------------------------------------------------------------------------
# Reference tile selection
# ---------------------------------------------------------------------------

def centre_rgb(rgb):
    """Return pixels from the central 16×16 region."""
    cx, cy = W // 4, H // 4
    pixels = []
    for y in range(cy, H - cy):
        for x in range(cx, W - cx):
            pixels.append(rgb[y * W + x])
    return pixels

def uniformity_score(rgb):
    """
    How similar are the edges to the centre?
    Lower = more uniform = more likely a fully-interior tile.
    """
    centre = centre_rgb(rgb)
    # average colour of centre
    rc = sum(p[0] for p in centre) / len(centre)
    gc = sum(p[1] for p in centre) / len(centre)
    bc = sum(p[2] for p in centre) / len(centre)
    centre_avg = (rc, gc, bc)

    total = 0.0
    for d in ['N', 'S', 'E', 'W']:
        for p in zone_pixels(rgb, d):
            total += math.sqrt((p[0]-rc)**2 + (p[1]-gc)**2 + (p[2]-bc)**2)
    return total

def find_reference_tile(tile_ids, tiles, pal):
    """Return the tile ID that is most uniform (best candidate for bitmask 255)."""
    best_id = None
    best_score = float('inf')
    for tid in tile_ids:
        if tid < 0 or tid >= len(tiles):
            continue
        rgb = tile_rgb(tiles[tid], pal)
        score = uniformity_score(rgb)
        if score < best_score:
            best_score = score
            best_id = tid
    return best_id


# ---------------------------------------------------------------------------
# Bitmask assignment
# ---------------------------------------------------------------------------

def assign_bitmask(tile_rgb, ref_rgb, threshold):
    """
    Compare each directional zone of tile_rgb against ref_rgb.
    Zone distance < threshold → neighbour present (bit = 1).
    Returns (bitmask, {direction: distance}).
    """
    bitmask = 0
    distances = {}
    for d in DIRECTIONS:
        tz = zone_pixels(tile_rgb, d)
        rz = zone_pixels(ref_rgb, d)
        dist = zone_distance(tz, rz)
        distances[d] = dist
        if dist < threshold:
            bitmask |= BIT[d]

    # Clamp diagonals: diagonal bit only valid when both adjacent cardinals set
    if not (bitmask & 1 and bitmask & 4):   bitmask &= ~2    # NE needs N and E
    if not (bitmask & 4 and bitmask & 16):  bitmask &= ~8    # SE needs E and S
    if not (bitmask & 16 and bitmask & 64): bitmask &= ~32   # SW needs S and W
    if not (bitmask & 1 and bitmask & 64):  bitmask &= ~128  # NW needs N and W

    return bitmask, distances


def canonical_bitmasks():
    blobs = set()
    for raw in range(256):
        n=bool(raw&1); ne=bool(raw&2); e=bool(raw&4); se=bool(raw&8)
        s=bool(raw&16); sw=bool(raw&32); w=bool(raw&64); nw=bool(raw&128)
        ne=ne and n and e; se=se and s and e; sw=sw and s and w; nw=nw and n and w
        blobs.add(n*1|ne*2|e*4|se*8|s*16|sw*32|w*64|nw*128)
    return blobs

CANONICAL = canonical_bitmasks()


# ---------------------------------------------------------------------------
# Auto-threshold calibration
# ---------------------------------------------------------------------------

def calibrate_threshold(tile_ids, tiles, pal, ref_id,
                        low=5.0, high=60.0, steps=20):
    """
    Find threshold that maximises coverage of canonical bitmasks while
    minimising collisions (multiple tiles mapping to the same bitmask).
    Returns (best_threshold, bitmask_dict).
    """
    ref_rgb = tile_rgb(tiles[ref_id], pal)
    best_thresh = low
    best_coverage = 0
    best_map = {}

    for step in range(steps + 1):
        thresh = low + (high - low) * step / steps
        bitmask_map = {}   # bitmask → (tile_id, avg_dist)

        for tid in tile_ids:
            if tid < 0 or tid >= len(tiles) or tid == ref_id:
                continue
            rgb = tile_rgb(tiles[tid], pal)
            bm, dists = assign_bitmask(rgb, ref_rgb, thresh)
            if bm not in CANONICAL:
                continue
            avg_dist = sum(dists.values()) / len(dists)
            if bm not in bitmask_map or avg_dist < bitmask_map[bm][1]:
                bitmask_map[bm] = (tid, avg_dist)

        # Always include the reference tile as bitmask 255
        bitmask_map[255] = (ref_id, 0.0)

        coverage = len(bitmask_map)
        if coverage > best_coverage:
            best_coverage = coverage
            best_thresh = thresh
            best_map = bitmask_map

    return best_thresh, best_map


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--tls',  required=True, help='Path to summer12mb.tls')
    ap.add_argument('--act',  default=None,  help='Path to netp.act palette')
    ap.add_argument('--json', required=True, help='Input autotile JSON')
    ap.add_argument('--out',  required=True, help='Output path stem (no extension)')
    ap.add_argument('--threshold', type=float, default=None,
                    help='Override edge-match threshold (default: auto-calibrate)')
    args = ap.parse_args()

    print(f"Loading {args.tls} ...")
    tiles, pal, tw, th = load_tls(args.tls, args.act)
    print(f"  {len(tiles)} tiles, {tw}×{th} px")

    with open(args.json) as f:
        doc = json.load(f)

    report_lines = []
    groups_out = []

    for grp in doc.get('groups', []):
        name = grp.get('name', '?')
        member_tiles = grp.get('member_tiles', [])
        valid = [t for t in member_tiles if 0 <= t < len(tiles)]

        print(f"\nGroup '{name}': {len(valid)} tiles")

        # Skip groups that already have bitmask data
        if grp.get('bitmask') and len(grp['bitmask']) > 5:
            print(f"  already has {len(grp['bitmask'])} bitmask entries — keeping")
            groups_out.append(grp)
            report_lines.append(f"{name}: kept existing ({len(grp['bitmask'])} entries)")
            continue

        if len(valid) < 2:
            print(f"  too few tiles, skipping")
            groups_out.append(grp)
            continue

        ref_id = find_reference_tile(valid, tiles, pal)
        print(f"  reference tile (bitmask 255): {ref_id}")

        if args.threshold is not None:
            thresh = args.threshold
            ref_rgb = tile_rgb(tiles[ref_id], pal)
            bm_map = {}
            for tid in valid:
                if tid == ref_id:
                    continue
                rgb = tile_rgb(tiles[tid], pal)
                bm, dists = assign_bitmask(rgb, ref_rgb, thresh)
                if bm not in CANONICAL:
                    continue
                avg_dist = sum(dists.values()) / len(dists)
                if bm not in bm_map or avg_dist < bm_map[bm][1]:
                    bm_map[bm] = (tid, avg_dist)
            bm_map[255] = (ref_id, 0.0)
        else:
            thresh, bm_map = calibrate_threshold(valid, tiles, pal, ref_id)
            print(f"  auto-calibrated threshold: {thresh:.1f}")

        coverage = len(bm_map)
        print(f"  bitmasks mapped: {coverage} / 47 canonical")

        # Build JSON bitmask object {str(bitmask): tile_id}
        bitmask_json = {str(bm): tid for bm, (tid, _) in sorted(bm_map.items())}

        grp_out = dict(grp)
        grp_out['bitmask'] = bitmask_json
        groups_out.append(grp_out)

        report = (f"{name}: ref={ref_id} thresh={thresh:.1f} "
                  f"coverage={coverage}/47")
        report_lines.append(report)

        # Show distribution of distances at calibrated threshold for diagnostics
        ref_rgb = tile_rgb(tiles[ref_id], pal)
        all_avg_dists = []
        for tid in valid:
            if tid >= len(tiles): continue
            rgb = tile_rgb(tiles[tid], pal)
            _, dists = assign_bitmask(rgb, ref_rgb, thresh)
            all_avg_dists.append(sum(dists.values()) / len(dists))
        if all_avg_dists:
            all_avg_dists.sort()
            p25 = all_avg_dists[len(all_avg_dists)//4]
            p75 = all_avg_dists[3*len(all_avg_dists)//4]
            print(f"  avg dist p25={p25:.1f} p75={p75:.1f}")

    doc_out = dict(doc)
    doc_out['groups'] = groups_out

    json_out = args.out + '.autotile.json'
    with open(json_out, 'w') as f:
        json.dump(doc_out, f, indent=2)
    print(f"\nWrote {json_out}")

    report_out = args.out + '.bitmask_report.txt'
    with open(report_out, 'w') as f:
        f.write('\n'.join(report_lines) + '\n')
    print(f"Wrote {report_out}")

if __name__ == '__main__':
    main()
