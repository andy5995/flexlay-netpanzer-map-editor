#include "tlsloader.h"
#include "npmformat.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <algorithm>

bool Tileset::load(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = f.readAll();

    // Minimum size check: header up to tile_count field
    if (data.size() < TLS_OFF_HEADERS)
        return false;

    const unsigned char* buf = reinterpret_cast<const unsigned char*>(data.constData());

    const uint16_t version    = uint16_t(buf[TLS_OFF_VERSION])     | (uint16_t(buf[TLS_OFF_VERSION+1]) << 8);
    const int      x_pix      = buf[TLS_OFF_X_PIX]  | (buf[TLS_OFF_X_PIX+1]  << 8);
    const int      y_pix      = buf[TLS_OFF_Y_PIX]  | (buf[TLS_OFF_Y_PIX+1]  << 8);
    const int      tile_count = buf[TLS_OFF_TILE_COUNT] | (buf[TLS_OFF_TILE_COUNT+1] << 8);
    (void)version; // checked implicitly via x_pix/y_pix

    if (x_pix != 32 || y_pix != 32 || tile_count <= 0)
        return false;

    // Parse palette (256 × RGB at TLS_OFF_PALETTE)
    QVector<QRgb> palette(256);
    for (int i = 0; i < 256; ++i) {
        const int base = TLS_OFF_PALETTE + i * 3;
        palette[i] = qRgb(buf[base], buf[base + 1], buf[base + 2]);
    }

    // The game renders tiles using an external netp.act palette, not the
    // TLS-embedded one. Walk up from the TLS file's directory to find it.
    {
        QDir dir = QFileInfo(path).absoluteDir();
        for (int attempt = 0; attempt < 5; ++attempt) {
            QFile actFile(dir.filePath("netp.act"));
            if (actFile.open(QIODevice::ReadOnly)) {
                const QByteArray actData = actFile.readAll();
                if (actData.size() >= 768) {
                    const auto* ab = reinterpret_cast<const unsigned char*>(actData.constData());
                    for (int i = 0; i < 256; ++i)
                        palette[i] = qRgb(ab[i*3], ab[i*3+1], ab[i*3+2]);
                }
                break;
            }
            if (!dir.cdUp()) break;
        }
    }

    // Tile headers immediately after fixed header block
    const int hdr_offset = TLS_OFF_HEADERS;
    const int px_offset  = hdr_offset + tile_count * 3;

    // Clamp tile_count to what's actually present in the file
    const qint64 available_pixels = data.size() - px_offset;
    const int pixels_per_tile = x_pix * y_pix;
    const int actual_count = std::min(tile_count,
                                      int(available_pixels / pixels_per_tile));
    if (actual_count <= 0)
        return false;

    QVector<TileHeader> headers(actual_count);
    for (int i = 0; i < actual_count; ++i) {
        const int off = hdr_offset + i * 3;
        headers[i].attrib     = int8_t(buf[off]);
        headers[i].move_value = int8_t(buf[off + 1]);
        headers[i].avg_color  = int8_t(buf[off + 2]);
    }

    const QByteArray rawPixels = data.mid(px_offset, actual_count * pixels_per_tile);

    // Commit
    m_tileW      = x_pix;
    m_tileH      = y_pix;
    m_tileCount  = actual_count;
    m_palette    = std::move(palette);
    m_headers    = std::move(headers);
    m_rawPixels  = rawPixels;
    m_atlas      = QImage();  // invalidate cached atlas
    m_atlasCols  = -1;
    return true;
}

QImage Tileset::tileImage(int i) const
{
    if (i < 0 || i >= m_tileCount)
        return QImage();

    QImage img(m_tileW, m_tileH, QImage::Format_RGB32);
    const int stride = m_tileW * m_tileH;
    const unsigned char* src =
        reinterpret_cast<const unsigned char*>(m_rawPixels.constData()) + i * stride;

    for (int y = 0; y < m_tileH; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < m_tileW; ++x)
            line[x] = m_palette[src[y * m_tileW + x]];
    }
    return img;
}

const QImage& Tileset::atlas(int cols) const
{
    if (m_atlasCols == cols && !m_atlas.isNull())
        return m_atlas;

    const int rows = (m_tileCount + cols - 1) / cols;
    QImage img(cols * m_tileW, rows * m_tileH, QImage::Format_RGB32);
    img.fill(Qt::black);

    const int stride = m_tileW * m_tileH;
    const unsigned char* rawSrc =
        reinterpret_cast<const unsigned char*>(m_rawPixels.constData());

    for (int i = 0; i < m_tileCount; ++i) {
        const int col = i % cols;
        const int row = i / cols;
        const int dx = col * m_tileW;
        const int dy = row * m_tileH;
        const unsigned char* src = rawSrc + i * stride;

        for (int y = 0; y < m_tileH; ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(dy + y)) + dx;
            for (int x = 0; x < m_tileW; ++x)
                line[x] = m_palette[src[y * m_tileW + x]];
        }
    }

    m_atlas     = std::move(img);
    m_atlasCols = cols;
    return m_atlas;
}

QRect Tileset::atlasRect(int i, int cols) const
{
    const int col = i % cols;
    const int row = i / cols;
    return QRect(col * m_tileW, row * m_tileH, m_tileW, m_tileH);
}
