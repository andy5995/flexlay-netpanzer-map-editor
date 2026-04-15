#include <QtTest/QtTest>
#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>
#include <cstdint>
#include <cstring>
#include <vector>

#include "tlsloader.h"
#include "npmformat.h"

// ---------------------------------------------------------------------------
// Helper: synthesise a minimal well-formed .tls binary.
//
// Built in a plain std::vector<uint8_t> so GCC's -Wstringop-overflow can
// see the full allocation size when we do pointer arithmetic on buf.data().
// (Using QByteArray::data() confuses GCC 12 because it tracks the pointer
// origin through QTypedArrayData's 24-byte header rather than the heap slab.)

static QByteArray makeTls(int tileCount, int tileW = 32, int tileH = 32)
{
    // Fixed header (840 bytes) + tileCount*3 tile headers + tileCount*tileW*tileH pixels
    const int pxOffset  = TLS_OFF_HEADERS + tileCount * 3;
    const int totalSize = pxOffset + tileCount * tileW * tileH;

    std::vector<uint8_t> buf(totalSize, 0);

    auto writeU16 = [&](int off, uint16_t v) {
        buf[off]     = v & 0xFF;
        buf[off + 1] = (v >> 8) & 0xFF;
    };

    // netp_id_header
    const char* hdr = "SyntheticTileset";
    std::memcpy(buf.data(), hdr, std::strlen(hdr));

    writeU16(TLS_OFF_VERSION,    1);
    writeU16(TLS_OFF_X_PIX,      uint16_t(tileW));
    writeU16(TLS_OFF_Y_PIX,      uint16_t(tileH));
    writeU16(TLS_OFF_TILE_COUNT, uint16_t(tileCount));

    // Palette: set a few distinctive entries at TLS_OFF_PALETTE (72), 256 × RGB
    uint8_t* pal = buf.data() + TLS_OFF_PALETTE;
    pal[0] = 0;   pal[1] = 0;   pal[2] = 0;    // entry 0 → black
    pal[3] = 255; pal[4] = 0;   pal[5] = 0;    // entry 1 → red
    pal[6] = 0;   pal[7] = 255; pal[8] = 0;    // entry 2 → green
    pal[255*3] = 255; pal[255*3+1] = 255; pal[255*3+2] = 255;  // entry 255 → white

    // Tile headers: give each tile a distinct move_value cycling 0..5
    for (int i = 0; i < tileCount; ++i) {
        int off = TLS_OFF_HEADERS + i * 3;
        buf[off]     = 0;              // attrib
        buf[off + 1] = uint8_t(i % 6);   // move_value
        buf[off + 2] = uint8_t(i % 256); // avg_color
    }

    // Pixel data: fill each tile with its own palette index (i % 256)
    uint8_t* px = buf.data() + pxOffset;
    const int pxPerTile = tileW * tileH;
    for (int i = 0; i < tileCount; ++i) {
        std::memset(px + i * pxPerTile, i % 256, size_t(pxPerTile));
    }

    return QByteArray(reinterpret_cast<char*>(buf.data()), totalSize);
}

static QString writeTempFile(const QByteArray& data, const QString& suffix)
{
    static QTemporaryDir tmpDir;
    static int counter = 0;
    const QString path = tmpDir.filePath(QString("tls_%1%2").arg(++counter).arg(suffix));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(data);
    return path;
}

// ---------------------------------------------------------------------------

class TestTlsLoader : public QObject {
    Q_OBJECT

private slots:

    // -----------------------------------------------------------------------
    void testLoad_basic()
    {
        const int N = 10;
        const QString path = writeTempFile(makeTls(N), ".tls");

        Tileset ts;
        QVERIFY(ts.load(path));
        QVERIFY(ts.isValid());
        QCOMPARE(ts.tileCount(), N);
        QCOMPARE(ts.tileW(), 32);
        QCOMPARE(ts.tileH(), 32);
    }

    // -----------------------------------------------------------------------
    void testPalette()
    {
        const QString path = writeTempFile(makeTls(1), ".tls");
        Tileset ts;
        QVERIFY(ts.load(path));

        const QVector<QRgb>& pal = ts.palette();
        QCOMPARE(pal.size(), 256);

        // entry 0 → black
        QCOMPARE(qRed(pal[0]),   0);
        QCOMPARE(qGreen(pal[0]), 0);
        QCOMPARE(qBlue(pal[0]),  0);
        // entry 1 → red
        QCOMPARE(qRed(pal[1]),   255);
        QCOMPARE(qGreen(pal[1]), 0);
        QCOMPARE(qBlue(pal[1]),  0);
        // entry 2 → green
        QCOMPARE(qRed(pal[2]),   0);
        QCOMPARE(qGreen(pal[2]), 255);
        QCOMPARE(qBlue(pal[2]),  0);
    }

    // -----------------------------------------------------------------------
    void testTileHeaders()
    {
        const int N = 8;
        const QString path = writeTempFile(makeTls(N), ".tls");
        Tileset ts;
        QVERIFY(ts.load(path));

        for (int i = 0; i < N; ++i) {
            QCOMPARE(int(ts.header(i).move_value), i % 6);
            QCOMPARE(int(uint8_t(ts.header(i).avg_color)), i % 256);
        }
    }

    // -----------------------------------------------------------------------
    void testTileImage()
    {
        // Tile i is filled with palette index (i % 256).
        // palette[i % 256] is set via our synthetic palette for i=0,1,2 → black, red, green.
        const QString path = writeTempFile(makeTls(3), ".tls");
        Tileset ts;
        QVERIFY(ts.load(path));

        // Tile 0: all pixels index 0 → black
        QImage img0 = ts.tileImage(0);
        QCOMPARE(img0.width(),  32);
        QCOMPARE(img0.height(), 32);
        QCOMPARE(img0.pixel(0, 0), qRgb(0, 0, 0));

        // Tile 1: all pixels index 1 → red
        QImage img1 = ts.tileImage(1);
        QCOMPARE(img1.pixel(0, 0), qRgb(255, 0, 0));

        // Tile 2: all pixels index 2 → green
        QImage img2 = ts.tileImage(2);
        QCOMPARE(img2.pixel(0, 0), qRgb(0, 255, 0));
    }

    // -----------------------------------------------------------------------
    void testAtlas_dimensions()
    {
        const int N = 10;
        const int COLS = 4;
        const QString path = writeTempFile(makeTls(N), ".tls");
        Tileset ts;
        QVERIFY(ts.load(path));

        const QImage& a = ts.atlas(COLS);
        const int expectedRows = (N + COLS - 1) / COLS; // ceil(10/4) = 3
        QCOMPARE(a.width(),  COLS * ts.tileW());
        QCOMPARE(a.height(), expectedRows * ts.tileH());
    }

    // -----------------------------------------------------------------------
    void testAtlas_content()
    {
        // Tile 0 → black, Tile 1 → red.
        // In a 2-column atlas, tile 0 is at (0,0) and tile 1 at (32, 0).
        const QString path = writeTempFile(makeTls(2), ".tls");
        Tileset ts;
        QVERIFY(ts.load(path));

        const int COLS = 2;
        const QImage& a = ts.atlas(COLS);

        // Tile 0 top-left corner → black
        const QRect r0 = ts.atlasRect(0, COLS);
        QCOMPARE(a.pixel(r0.left(), r0.top()), qRgb(0, 0, 0));

        // Tile 1 top-left corner → red
        const QRect r1 = ts.atlasRect(1, COLS);
        QCOMPARE(a.pixel(r1.left(), r1.top()), qRgb(255, 0, 0));
    }

    // -----------------------------------------------------------------------
    void testAtlasRect()
    {
        const int N = 5, COLS = 3, W = 32, H = 32;
        const QString path = writeTempFile(makeTls(N, W, H), ".tls");
        Tileset ts; ts.load(path);

        // tile 0 → col 0, row 0
        QCOMPARE(ts.atlasRect(0, COLS), QRect(0, 0, W, H));
        // tile 1 → col 1, row 0
        QCOMPARE(ts.atlasRect(1, COLS), QRect(W, 0, W, H));
        // tile 3 → col 0, row 1
        QCOMPARE(ts.atlasRect(3, COLS), QRect(0, H, W, H));
        // tile 4 → col 1, row 1
        QCOMPARE(ts.atlasRect(4, COLS), QRect(W, H, W, H));
    }

    // -----------------------------------------------------------------------
    void testInvalidFile()
    {
        Tileset ts;
        QVERIFY(!ts.load("/nonexistent/path.tls"));
        QVERIFY(!ts.isValid());
    }

    // -----------------------------------------------------------------------
    void testTruncatedFile()
    {
        // Cut the file short — loader should either fail or clamp tile count
        const QByteArray full = makeTls(100);
        const QByteArray truncated = full.left(full.size() / 2);
        const QString path = writeTempFile(truncated, ".tls");

        Tileset ts;
        // Should either fail or load fewer tiles — not crash
        if (ts.load(path))
            QVERIFY(ts.tileCount() < 100);
        // No crash → pass
    }

    // -----------------------------------------------------------------------
    void testAtlasCaching()
    {
        // 4 tiles arranged in 2 cols → 2×32 wide, 2×32 tall
        // 4 tiles arranged in 4 cols → 4×32 wide, 1×32 tall
        const QString path = writeTempFile(makeTls(4), ".tls");
        Tileset ts; ts.load(path);

        const QImage& a2 = ts.atlas(2);
        QCOMPARE(a2.width(),  2 * 32);
        QCOMPARE(a2.height(), 2 * 32);

        // Calling atlas(2) again must return identical dimensions (cached)
        const QImage& a2b = ts.atlas(2);
        QCOMPARE(a2b.width(),  a2.width());
        QCOMPARE(a2b.height(), a2.height());

        // Different col count → different atlas dimensions
        const QImage& a4 = ts.atlas(4);
        QCOMPARE(a4.width(),  4 * 32);
        QCOMPARE(a4.height(), 1 * 32);
    }
};

QTEST_MAIN(TestTlsLoader)
#include "test_tlsloader.moc"
