#!/usr/bin/env python3
"""Convert netpanzerbrushes.scm from the Flexlay editor to Qt editor stamp JSON files.

Usage:
    python3 tools/import_flexlay_brushes.py \
        --scm netpanzer/netpanzerbrushes.scm \
        --out-dir data/stamps
"""

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path

# Map brush name prefix → output file stem
CATEGORY_MAP = {
    "House":           "houses",
    "Small House":     "houses",
    "Mountain":        "mountains",
    "Dirt":            "dirt",
    "Lake":            "lakes",
    "Outpost":         "outpost_flexlay",
    "Road":            "roads",
    "River Cross":     "river_cross",
    "River":           "rivers",
    "Trees":           "trees",
}

def categorise(name):
    for prefix, cat in CATEGORY_MAP.items():
        if name.startswith(prefix):
            return cat
    return "misc"


def parse_scm(path):
    text = Path(path).read_text()
    entries = re.findall(r'\((\d+)\s+(\d+)\s+(\d+)\s+"([^"]+)"\)', text)
    return [(int(start), int(w), int(h), name) for start, w, h, name in entries]


def build_stamp(start, width, height, name):
    tiles = list(range(start, start + width * height))
    return {"name": name, "width": width, "height": height, "tiles": tiles}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scm", default="netpanzer/netpanzerbrushes.scm")
    parser.add_argument("--out-dir", default="data/stamps")
    args = parser.parse_args()

    brushes = parse_scm(args.scm)
    print(f"Parsed {len(brushes)} brushes")

    # De-duplicate names within each category by appending a counter
    by_cat = defaultdict(list)
    cat_name_count = defaultdict(lambda: defaultdict(int))

    for start, w, h, name in brushes:
        cat = categorise(name)
        cat_name_count[cat][name] += 1

    # Build stamps, numbering duplicates
    cat_name_seen = defaultdict(lambda: defaultdict(int))
    for start, w, h, name in brushes:
        cat = categorise(name)
        cat_name_seen[cat][name] += 1
        display_name = name
        if cat_name_count[cat][name] > 1:
            display_name = f"{name} {cat_name_seen[cat][name]}"
        by_cat[cat].append(build_stamp(start, w, h, display_name))

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    for cat, stamps in sorted(by_cat.items()):
        out_path = out_dir / f"{cat}.stamp.json"
        with open(out_path, "w") as f:
            json.dump({"stamps": stamps}, f, indent=2)
        print(f"  {out_path}  ({len(stamps)} stamps)")


if __name__ == "__main__":
    main()
