# NetPanzer Map Editor

A Qt5-based map editor for the [netPanzer](https://netpanzer.io) open-source real-time strategy game.

## Status

The editor is functional but still early in development.  Creating a polished
map from scratch is challenging — there is no flood-fill or per-tile attribute
editing yet.

The most practical workflow is to open an existing `.npm` map, reshape terrain
and structures with the tools below, and save it under a new name.

## Features

- Open and save netPanzer `.npm` maps (binary format, verified round-trip) — see [docs/map-format.md](docs/map-format.md)
- Add new tiles to any `.tls` tileset via `tools/add_tile.py` — see [docs/adding-tiles.md](docs/adding-tiles.md)
- Tile painting with full undo/redo
- Ellipse paint tool — drag to paint tiles along an ellipse outline
- Rect select and stamp system — drag-select a region, save as a stamp, place
  with one click; pre-built stamps auto-loaded from `data/stamps/` at startup;
  Over 100 preset stamps (houses, mountains, roads, rivers, trees, lakes, outpost
  structures) imported from the original Flexlay editor via
  `tools/import_flexlay_brushes.py`
- Tile pick tool — click any map tile to select it for painting
- Autotile — automatic transition-tile selection based on 8-neighbour bitmask;
  enabled when a `.autotile.json` sidecar is found for the loaded tileset
  (see [docs/autotile-format.md](docs/autotile-format.md))
- summer12mb autotile support — bitmask mappings derived from tile imagery via
  `tools/derive_autotile_bitmasks.py`; mountain/water/wall/road/grass/river covered
- Tileset auto-detected when opening a map or creating a new one
- Tile browser with jump-to-ID search
- Place and move outpost and spawn-point objects
- Object renaming via double-click or context menu
- Minimap with viewport overlay and click-to-pan
- Zoom toward cursor
- Keyboard shortcuts: `T` (tile paint), `E` (ellipse), `U` (rect outline), `I` (pick),
  `R` (rect select), `F` (rect fill), `M` (stamp), `O` (place outpost), `S` (spawn point),
  `V` (select object), `G` (grid)

## Workflows

### Capture a map region as a stamp and save it

1. Press `R` to switch to the **Rect Select** tool.
2. Drag across the map to select the region you want to capture. A yellow
   highlight shows the selection.
3. Open the **Stamps** panel (View → Stamps if it isn't visible).
4. Click **Capture Selection** — the region is added as a new stamp.
5. Click the stamp to select it, then click **Save…** to write it to a
   `.stamp.json` file. Saved stamps can be reloaded with **Load…** in any
   future session.

To place the stamp, press `M` (Stamp Paint) and click on the map.

### Load stamps from a directory

Stamps in `data/stamps/` are loaded automatically at startup. You can drop
additional `.stamp.json` files there, or use **Load…** in the Stamps panel to
load them from any location.

### Drag-select tiles in the tile browser

You can also drag-select a rectangular region in the **Tile Browser** to create
a multi-tile stamp directly from the tileset — release the mouse to send it
straight to the active stamp.

## Building

See [BUILD.md](BUILD.md) for full build and AppImage instructions.

Quick start:

```sh
meson setup build
ninja -C build
./build/netpanzer-editor
```

## License

GNU GPL v3 — see [COPYING](COPYING) for details.
Original Flexlay code by Ingo Ruhnke &lt;grumbel@gmx.de&gt;.
Qt5 port by andy5995.
