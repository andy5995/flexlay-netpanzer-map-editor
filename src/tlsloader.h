#pragma once
#include <QString>
#include <QVector>
#include <QRgb>
#include <QByteArray>
#include <QImage>
#include <QRect>

// Per-tile metadata from the .tls header array
struct TileHeader {
    int8_t attrib;       // tile attribute flags
    int8_t move_value;   // 0=road, 1=normal, 4=impassable, 5=water
    int8_t avg_color;    // palette index of the average tile colour
};

// Loaded netPanzer tileset (.tls).
// Tiles are 32×32 pixels, palette-indexed (256 colours).
// Atlas: all tiles arranged left-to-right, top-to-bottom in a grid.
class Tileset {
public:
    bool load(const QString& path);
    bool isValid() const { return m_tileCount > 0 && !m_rawPixels.isEmpty(); }

    int tileCount() const { return m_tileCount; }
    int tileW()     const { return m_tileW; }
    int tileH()     const { return m_tileH; }

    const TileHeader& header(int i) const { return m_headers[i]; }
    const QVector<QRgb>& palette() const { return m_palette; }

    // Build (lazily cached) atlas QImage: all tiles arranged in `cols` columns.
    const QImage& atlas(int cols = 64) const;

    // Source rect inside atlas(cols) for tile id `i`.
    QRect atlasRect(int i, int cols = 64) const;

    // Decode a single tile to an ARGB32 QImage (not cached — use atlas for rendering).
    QImage tileImage(int i) const;

private:
    int m_tileCount = 0;
    int m_tileW = 32;
    int m_tileH = 32;
    QVector<TileHeader> m_headers;
    QVector<QRgb> m_palette;  // 256 entries
    QByteArray m_rawPixels;   // m_tileCount × m_tileW × m_tileH bytes

    mutable QImage m_atlas;
    mutable int m_atlasCols = -1;
};
