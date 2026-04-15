#pragma once
#include <cstdint>

// .npm binary file layout (little-endian)
// All offsets are byte positions from the start of the file.
//
// Based on MapFile.hpp from netpanzer source:
//   char     netp_id_header[64]
//   uint16   id
//   char     name[256]
//   char     description[1024]
//   uint16   width
//   uint16   height
//   char     tile_set[256]
//   uint16   thumbnail_width
//   uint16   thumbnail_height
// Then:
//   uint16   tiles[width * height]     (LE, starting at NPM_OFF_TILES)
//   uint8    thumbnail[thumbW * thumbH] (palette-indexed, may be absent)

static constexpr int NPM_HEADER_ID_SIZE   = 64;
static constexpr int NPM_NAME_SIZE        = 256;
static constexpr int NPM_DESC_SIZE        = 1024;
static constexpr int NPM_TILESET_SIZE     = 256;

static constexpr int NPM_OFF_ID_HEADER    = 0;
static constexpr int NPM_OFF_ID           = 64;        // uint16
static constexpr int NPM_OFF_NAME         = 66;        // char[256]
static constexpr int NPM_OFF_DESCRIPTION  = 322;       // char[1024]
static constexpr int NPM_OFF_WIDTH        = 1346;      // uint16
static constexpr int NPM_OFF_HEIGHT       = 1348;      // uint16
static constexpr int NPM_OFF_TILESET      = 1350;      // char[256]
static constexpr int NPM_OFF_THUMB_W      = 1606;      // uint16
static constexpr int NPM_OFF_THUMB_H      = 1608;      // uint16
static constexpr int NPM_OFF_TILES        = 1610;      // uint16[width*height]

static constexpr int NPM_MIN_SIZE = NPM_OFF_TILES + 4; // at least 1 tile

// .tls binary file layout
//   unsigned char  netp_id_header[64]
//   uint16         version        (= 1)
//   uint16         x_pix          (= 32)
//   uint16         y_pix          (= 32)
//   uint16         tile_count
//   unsigned char  palette[768]   (256 × RGB)
//   TILE_HEADER    headers[tile_count]  (3 bytes each: attrib, move_value, avg_color)
//   unsigned char  pixels[tile_count * x_pix * y_pix]  (palette-indexed)

static constexpr int TLS_OFF_VERSION    = 64;
static constexpr int TLS_OFF_X_PIX     = 66;
static constexpr int TLS_OFF_Y_PIX     = 68;
static constexpr int TLS_OFF_TILE_COUNT = 70;
static constexpr int TLS_OFF_PALETTE   = 72;   // 768 bytes
static constexpr int TLS_OFF_HEADERS   = 840;  // 72 + 768 = 840
