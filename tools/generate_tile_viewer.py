#!/usr/bin/env python3
"""
Generate an interactive HTML tile viewer for a netPanzer .tls tileset.

Usage:
    python3 tools/generate_tile_viewer.py [TLS_FILE] [ACT_FILE] [OUTPUT_HTML]

Defaults (uses NETPANZER_DATADIR env var if set):
    TLS_FILE   = $NETPANZER_DATADIR/wads/summer12mb/SummerDay/summer12mb.tls
    ACT_FILE   = $NETPANZER_DATADIR/wads/netp.act
    OUTPUT_HTML = /tmp/tile_viewer.html

The viewer shows every tile at 2x scale, labeled with its ID. Border colour
indicates move_value: green=passable(4), yellow=slow(1), red=blocking(0),
cyan=water(5). Click a tile to copy its ID to the clipboard. Use the filter
checkboxes to hide/show tiles by move_value, and the jump box to scroll to a
specific tile ID.
"""

import os
import struct
import base64
import io
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required: pip install Pillow")


def default_paths():
    datadir = os.environ.get(
        "NETPANZER_DATADIR",
        os.path.expanduser("~/src/netpanzer/data"),
    )
    tls = os.path.join(datadir, "wads", "summer12mb", "SummerDay", "summer12mb.tls")
    act = os.path.join(datadir, "wads", "netp.act")
    return tls, act


def load_tls(tls_path, act_path):
    with open(tls_path, "rb") as f:
        f.read(64)  # id header
        struct.unpack("<H", f.read(2))  # version
        x_pix = struct.unpack("<H", f.read(2))[0]
        y_pix = struct.unpack("<H", f.read(2))[0]
        tile_count = struct.unpack("<H", f.read(2))[0]
        f.read(768)  # embedded palette (unused — we use netp.act)
        headers = [struct.unpack("bbb", f.read(3)) for _ in range(tile_count)]
        tiles = [f.read(x_pix * y_pix) for _ in range(tile_count)]

    with open(act_path, "rb") as f:
        palette = list(f.read(768))

    return x_pix, y_pix, tile_count, headers, tiles, palette


def tile_to_b64(pixel_data, palette, x_pix, y_pix, scale=2):
    img = Image.frombytes("P", (x_pix, y_pix), pixel_data)
    img.putpalette(palette)
    img = img.convert("RGB").resize((x_pix * scale, y_pix * scale), Image.NEAREST)
    buf = io.BytesIO()
    img.save(buf, "PNG")
    return base64.b64encode(buf.getvalue()).decode()


MV_COLOR = {0: "#c00", 1: "#880", 4: "#060", 5: "#066"}
MV_LABEL = {0: "block(0)", 1: "slow(1)", 4: "pass(4)", 5: "water(5)"}


def generate(tls_path, act_path, output_path):
    print(f"Loading {tls_path} …")
    x_pix, y_pix, tile_count, headers, tiles, palette = load_tls(tls_path, act_path)
    print(f"  {tile_count} tiles ({x_pix}×{y_pix})")

    tw, th = x_pix * 2, y_pix * 2  # display size

    print(f"Generating {output_path} …")
    with open(output_path, "w") as out:
        out.write(f"""<!DOCTYPE html>
<html><head><meta charset="utf-8">
<title>netPanzer Tile Viewer — {os.path.basename(tls_path)}</title>
<style>
body{{background:#111;color:#eee;font-family:monospace;font-size:11px;margin:0}}
#sidebar{{position:fixed;top:0;right:0;background:#1e1e1e;padding:10px;width:190px;
  height:100vh;box-sizing:border-box;overflow-y:auto;border-left:1px solid #444}}
#grid{{display:flex;flex-wrap:wrap;gap:2px;padding:4px;margin-right:200px}}
.tile{{display:inline-block;text-align:center;cursor:pointer;border:2px solid #333}}
.tile:hover{{border-color:#fff!important}}
.tile img{{display:block}}
.tid{{font-size:9px;padding:1px 0;background:#000;user-select:none}}
.mv0{{border-color:#c00}}.mv1{{border-color:#880}}
.mv4{{border-color:#060}}.mv5{{border-color:#066}}
h3{{margin:4px 0 8px}}
#sel{{background:#333;padding:4px;margin:4px 0;min-height:1.4em}}
label{{display:block;margin:2px 0}}
input[type=number]{{width:70px;background:#333;color:#eee;border:1px solid #555;padding:2px}}
button{{background:#444;color:#eee;border:1px solid #666;padding:2px 6px;cursor:pointer}}
button:hover{{background:#555}}
hr{{border-color:#444;margin:8px 0}}
</style></head><body>
<div id="sidebar">
  <h3>Tile Viewer</h3>
  <div>Click tile → copy ID</div>
  <div id="sel">—</div>
  <hr>
  <b>Filter move_value:</b><br>
""")
        for mv, label in MV_LABEL.items():
            color = MV_COLOR[mv]
            out.write(
                f'  <label><input type="checkbox" class="mvfilter" value="{mv}" checked>'
                f' <span style="color:{color}">{label}</span></label>\n'
            )
        out.write(f"""  <hr>
  <b>Jump to tile ID:</b><br>
  <input id="jumpid" type="number" min="0" max="{tile_count-1}">
  <button onclick="jumpTo()">Go</button>
</div>
<div id="grid">
""")

        for tid in range(tile_count):
            attrib, mv, avg = headers[tid]
            b64 = tile_to_b64(tiles[tid], palette, x_pix, y_pix)
            color = MV_COLOR.get(mv, "#555")
            out.write(
                f'<div class="tile mv{mv}" data-mv="{mv}" data-tid="{tid}"'
                f' onclick="sel({tid})" style="border-color:{color}">'
                f'<img src="data:image/png;base64,{b64}" width="{tw}" height="{th}">'
                f'<div class="tid">{tid}</div></div>\n'
            )
            if tid % 500 == 0:
                print(f"  {tid}/{tile_count}", end="\r", flush=True)

        out.write(f"""</div>
<script>
function sel(id){{
  document.getElementById("sel").textContent="ID: "+id;
  navigator.clipboard&&navigator.clipboard.writeText(String(id));
}}
function jumpTo(){{
  var id=parseInt(document.getElementById("jumpid").value);
  var el=document.querySelector("[data-tid='"+id+"']");
  if(el)el.scrollIntoView({{block:"center"}});
}}
document.querySelectorAll(".mvfilter").forEach(function(cb){{
  cb.addEventListener("change",function(){{
    var show=cb.checked;
    document.querySelectorAll(".tile[data-mv='"+cb.value+"']")
      .forEach(function(t){{t.style.display=show?"inline-block":"none";}});
  }});
}});
</script>
</body></html>
""")

    size_mb = os.path.getsize(output_path) / 1024 / 1024
    print(f"\nDone — {output_path} ({size_mb:.1f} MB, {tile_count} tiles)")


if __name__ == "__main__":
    args = sys.argv[1:]
    tls_default, act_default = default_paths()
    tls_path   = args[0] if len(args) > 0 else tls_default
    act_path   = args[1] if len(args) > 1 else act_default
    output     = args[2] if len(args) > 2 else "/tmp/tile_viewer.html"
    generate(tls_path, act_path, output)
