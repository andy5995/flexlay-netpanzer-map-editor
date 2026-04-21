#!/usr/bin/env python3
"""Generate river-to-water transition tiles for all summer12mb tileset variants.

This script is specific to the summer12mb tileset family.  It blends the three
river edge tiles (IDs 9467–9469) into the matching deep-water tiles (IDs
10967–10969) across the full tile height, then appends the results as new tiles
to each variant's .tls file.

The generated tile IDs are:
  Desert    — 11960, 11961, 11962  (Desert base count is 11960)
  all others — 11961, 11962, 11963

Usage:
    python3 tools/gen_river_water_transitions.py --wads /path/to/netpanzer/data/wads

A .tls.bak backup is written beside each .tls before any changes are made.
"""

import argparse
import pathlib
import struct
from collections import Counter

import numpy as np

RIVER_IDS  = [9467, 9468, 9469]
WATER_IDS  = [10967, 10968, 10969]
# Original tile counts before any transition tiles were added
BASE_COUNTS  = {'Desert': 11960}   # Desert is missing the placeholder at 11960
DEFAULT_BASE = 11961
# All variants should reach 11961 before transition tiles are added.
# Desert is missing the empty placeholder tile at 11960; we insert it here
# so transition tiles land at 11961-11963 across all variants.
NOTILE_PIXELS = bytes([1] * 1024)  # near-black placeholder, matches other variants


def load_act(wads: pathlib.Path) -> np.ndarray:
    return np.frombuffer((wads / 'netp.act').read_bytes()[:768], dtype=np.uint8).reshape(256, 3)


def extract_tile_rgb(data: bytes, tid: int, count: int, act_pal: np.ndarray) -> np.ndarray:
    px_base = 840 + count * 3
    raw = np.frombuffer(data[px_base + tid*1024 : px_base + tid*1024 + 1024], dtype=np.uint8)
    return act_pal[raw].reshape(32, 32, 3).astype(np.float32)


def blend_transition(river: np.ndarray, water: np.ndarray) -> np.ndarray:
    """Linear blend from river (top) to water (bottom) across the full tile."""
    h = river.shape[0]
    out = river.copy().astype(np.float32)
    for row in range(h):
        t = row / (h - 1)
        out[row] = (1.0 - t) * river[row] + t * water[row]
    return np.clip(out, 0, 255).astype(np.uint8)


def quantize(rgb: np.ndarray, allowed_indices: list, act_pal: np.ndarray) -> bytes:
    sub_pal = act_pal[allowed_indices].astype(np.int32)
    pixels  = rgb.reshape(-1, 3).astype(np.int32)
    diff    = pixels[:, np.newaxis, :] - sub_pal[np.newaxis, :, :]
    nearest = (diff * diff).sum(axis=2).argmin(axis=1)
    return bytes(np.array(allowed_indices, dtype=np.uint8)[nearest].tobytes())


def add_tile(data: bytearray, pixels: bytes) -> int:
    count     = struct.unpack_from('<H', data, 70)[0]
    px_offset = 840 + count * 3
    avg_col   = Counter(pixels).most_common(1)[0][0]
    header    = bytes([0, 0, avg_col])
    new_data  = data[:px_offset] + header + bytes(data[px_offset:]) + pixels
    struct.pack_into('<H', new_data, 70, count + 1)
    data[:]   = new_data
    return count  # new tile id


def strip_to(data: bytearray, target: int):
    count = struct.unpack_from('<H', data, 70)[0]
    if count <= target:
        return
    new_data = bytearray(
        bytes(data[:840])
        + bytes(data[840 : 840 + target * 3])
        + bytes(data[840 + count * 3 : 840 + count * 3 + target * 1024])
    )
    struct.pack_into('<H', new_data, 70, target)
    data[:] = new_data


def process_variant(tls_path: pathlib.Path, act_pal: np.ndarray):
    name = tls_path.parent.name
    data = bytearray(tls_path.read_bytes())
    count = struct.unpack_from('<H', data, 70)[0]

    base = BASE_COUNTS.get(name, DEFAULT_BASE)
    if count > base:
        strip_to(data, base)
        count = base

    # Ensure all variants are at DEFAULT_BASE before adding transition tiles
    if count < DEFAULT_BASE:
        add_tile(data, NOTILE_PIXELS)
        count = struct.unpack_from('<H', data, 70)[0]

    # Palette indices used by the river and water tiles — keeps new tiles
    # visually consistent with their neighbours
    allowed = set()
    for tid in RIVER_IDS + WATER_IDS:
        if tid < count:
            px_base = 840 + count * 3
            allowed.update(data[px_base + tid * 1024 : px_base + tid * 1024 + 1024])
    allowed_list = sorted(allowed)

    # Compute median interior water colour (excluding dark bank/edge pixels).
    cur = struct.unpack_from('<H', data, 70)[0]
    water_pixels = []
    for w_id in WATER_IDS:
        if w_id < cur:
            rgb = extract_tile_rgb(bytes(data), w_id, cur, act_pal)
            mask = rgb.mean(axis=2) >= 15
            water_pixels.append(rgb[mask])
    water_fill = np.median(np.concatenate(water_pixels), axis=0) if water_pixels else np.array([10., 10., 10.])

    new_ids = []
    for r_id, w_id in zip(RIVER_IDS, WATER_IDS):
        if r_id >= count or w_id >= count:
            print(f'  {name}: skipping {r_id}/{w_id} — out of range')
            continue
        cur = struct.unpack_from('<H', data, 70)[0]
        river = extract_tile_rgb(bytes(data), r_id, cur, act_pal)
        water = extract_tile_rgb(bytes(data), w_id, cur, act_pal)
        blended = blend_transition(river, water)
        pixels  = quantize(blended, allowed_list, act_pal)
        new_id  = add_tile(data, pixels)
        new_ids.append(new_id)

    tls_path.with_suffix('.tls.bak').write_bytes(tls_path.read_bytes())
    tls_path.write_bytes(data)
    print(f'  {name}: transition tiles {new_ids}')


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--wads', required=True,
                    help='Path to netpanzer data/wads directory')
    args = ap.parse_args()

    wads = pathlib.Path(args.wads)
    act_pal = load_act(wads)

    variants = sorted((wads / 'summer12mb').glob('*/summer12mb.tls'))
    if not variants:
        ap.error(f'No summer12mb variants found under {wads}/summer12mb/')

    print(f'Processing {len(variants)} variants...')
    for tls_path in variants:
        process_variant(tls_path, act_pal)
    print('Done.')


if __name__ == '__main__':
    main()
