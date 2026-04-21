# NetPanzer Map Editor

A Qt5-based map editor for the [netPanzer](https://netpanzer.io) open-source real-time strategy game.

## Status

The editor is functional but still early in development.  Creating a polished
map from scratch is challenging — there is no flood-fill or per-tile attribute
editing yet.

That said, there are quite a few ways to work with existing maps:

- **Autotile painting** — paint terrain that blends smoothly with its neighbours
- **Ellipse tool** — quickly outline roads, rivers, or crater rims
- **Rect select + stamp** — copy any region of an existing map and paste it
  elsewhere, or save it as a named stamp for repeated use
- **Pre-built stamps** — 139 preset stamps across houses, mountains, roads,
  rivers, trees, dirt patches, lakes, and outpost structures, imported from
  the original Flexlay editor
- **Tile pick** — sample any tile already on the map and paint with it
- **Object editing** — move, rename, add, or remove outposts and spawn points
- **Minimap navigation** — pan quickly around large maps

The most practical workflow is to open an existing `.npm` map, reshape terrain
and structures with the tools above, and save it under a new name.

## Features

- Open and save netPanzer `.npm` maps (binary format, verified round-trip) — see [docs/map-format.md](docs/map-format.md)
- Add new tiles to any `.tls` tileset via `tools/add_tile.py` — see [docs/adding-tiles.md](docs/adding-tiles.md)
- Tile painting with full undo/redo
- Ellipse paint tool — drag to paint tiles along an ellipse outline
- Rect select and stamp system — drag-select a region, save as a stamp, place
  with one click; pre-built stamps auto-loaded from `data/stamps/` at startup;
  139 preset stamps (houses, mountains, roads, rivers, trees, lakes, outpost
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
