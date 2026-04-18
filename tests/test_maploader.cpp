#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QByteArray>
#include <cstring>
#include <cstdint>

#include "maploader.h"
#include "npmformat.h"

// ---------------------------------------------------------------------------
// Helper: write a well-formed .npm binary with the given tiles.

static QByteArray makeNpm(int w, int h,
                           const std::vector<uint16_t>& tiles,
                           const QString& name    = "TestMap",
                           const QString& tileset = "summer12mb.tls")
{
    const qint64 tileCount = qint64(w) * h;
    const qint64 total     = NPM_OFF_TILES + tileCount * 2;
    QByteArray buf(int(total), '\0');

    auto writeU16 = [&](int off, uint16_t v) {
        buf[off]     = char(v & 0xFF);
        buf[off + 1] = char((v >> 8) & 0xFF);
    };
    auto writeStr = [&](int off, int len, const QString& s) {
        QByteArray latin = s.toLatin1();
        int n = qMin(latin.size(), len - 1);
        for (int i = 0; i < n; ++i) buf[off + i] = latin[i];
    };

    writeStr(NPM_OFF_ID_HEADER, NPM_HEADER_ID_SIZE, "Copyright PyroSoft Inc.");
    writeU16(NPM_OFF_ID,     1);
    writeStr(NPM_OFF_NAME,        NPM_NAME_SIZE,  name);
    writeStr(NPM_OFF_DESCRIPTION, NPM_DESC_SIZE,  "A test map.");
    writeU16(NPM_OFF_WIDTH,  uint16_t(w));
    writeU16(NPM_OFF_HEIGHT, uint16_t(h));
    writeStr(NPM_OFF_TILESET, NPM_TILESET_SIZE, tileset);
    writeU16(NPM_OFF_THUMB_W, 0);
    writeU16(NPM_OFF_THUMB_H, 0);

    for (qint64 i = 0; i < tileCount; ++i) {
        uint16_t v = (i < (qint64)tiles.size()) ? tiles[size_t(i)] : 0;
        writeU16(int(NPM_OFF_TILES + i * 2), v);
    }
    return buf;
}

// Helper: write buf to a temp file and return the path.
static QString writeTempFile(const QByteArray& data, const QString& suffix)
{
    // QTemporaryFile auto-deletes on destruction; we manage lifetime via the dir.
    static QTemporaryDir tmpDir;
    static int counter = 0;
    const QString path = tmpDir.filePath(
        QString("test_%1%2").arg(++counter).arg(suffix));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return {};
    f.write(data);
    return path;
}

// ---------------------------------------------------------------------------

class TestMapLoader : public QObject {
    Q_OBJECT

private slots:
    // -----------------------------------------------------------------------
    void testLoadNpm_basic()
    {
        const int W = 4, H = 3;
        const std::vector<uint16_t> tiles = {
            100, 200, 300, 400,
            500, 600, 700, 800,
            900,1000,1100,1200
        };
        const QByteArray npm = makeNpm(W, H, tiles);
        const QString path   = writeTempFile(npm, ".npm");

        const Map m = MapLoader::load(path);
        QVERIFY(m.isValid());
        QCOMPARE(m.width,  W);
        QCOMPARE(m.height, H);
        QCOMPARE((int)m.tiles.size(), W * H);
        for (int i = 0; i < W * H; ++i)
            QCOMPARE(int(m.tiles[size_t(i)]), int(tiles[size_t(i)]));
    }

    // -----------------------------------------------------------------------
    void testLoadNpm_metadata()
    {
        const QByteArray npm = makeNpm(8, 8, {}, "MyMap", "custom.tls");
        const QString path   = writeTempFile(npm, ".npm");

        const Map m = MapLoader::load(path);
        QVERIFY(m.isValid());
        QCOMPARE(m.name,        QString("MyMap"));
        QCOMPARE(m.tileSetName, QString("custom.tls"));
        QCOMPARE(m.id,          uint16_t(1));
    }

    // -----------------------------------------------------------------------
    void testLoadNpm_tooSmall()
    {
        // A truncated file (no tile data) should fail gracefully
        const QByteArray tiny("abc", 3);
        const QString path = writeTempFile(tiny, ".npm");

        const Map m = MapLoader::load(path);
        QVERIFY(!m.isValid());
    }

    // -----------------------------------------------------------------------
    void testLoadNpm_zeroDimensions()
    {
        // Width/height both 0 — should not produce a valid map.
        const QByteArray npm = makeNpm(0, 0, {});
        // The buffer is exactly NPM_OFF_TILES (no tile bytes), which is fine for a
        // zero-tile map — but isValid() must return false.
        const QString path = writeTempFile(npm, ".npm");

        const Map m = MapLoader::load(path);
        QVERIFY(!m.isValid());
    }

    // -----------------------------------------------------------------------
    void testLoadText_basic()
    {
        const QString text = "3 2\n10 20 30\n40 50 60\n";
        const QString path = writeTempFile(text.toUtf8(), ".txt");

        const Map m = MapLoader::load(path);
        QVERIFY(m.isValid());
        QCOMPARE(m.width,  3);
        QCOMPARE(m.height, 2);
        QCOMPARE(int(m.tiles[0]), 10);
        QCOMPARE(int(m.tiles[5]), 60);
    }

    // -----------------------------------------------------------------------
    void testSaveText_roundTrip()
    {
        // Build a small map with known tiles
        Map orig;
        orig.width  = 4;
        orig.height = 3;
        orig.tiles  = {1,2,3,4, 5,6,7,8, 9,10,11,12};

        static QTemporaryDir tmpDir;
        const QString path = tmpDir.filePath("roundtrip.txt");

        QVERIFY(MapLoader::saveText(path, orig));

        const Map loaded = MapLoader::load(path);
        QVERIFY(loaded.isValid());
        QCOMPARE(loaded.width,  orig.width);
        QCOMPARE(loaded.height, orig.height);
        for (int i = 0; i < orig.width * orig.height; ++i)
            QCOMPARE(int(loaded.tiles[size_t(i)]), int(orig.tiles[size_t(i)]));
    }

    // -----------------------------------------------------------------------
    void testSaveNpm_roundTrip()
    {
        Map orig;
        orig.width      = 6;
        orig.height     = 5;
        orig.tiles.resize(size_t(orig.width * orig.height));
        for (size_t i = 0; i < orig.tiles.size(); ++i)
            orig.tiles[i] = uint16_t(i * 7 % 500); // some pattern
        orig.name       = "RoundTripMap";
        orig.tileSetName = "summer12mb.tls";
        orig.id         = 42;

        static QTemporaryDir tmpDir;
        const QString path = tmpDir.filePath("roundtrip.npm");

        QVERIFY(MapLoader::saveNpm(path, orig));

        const Map loaded = MapLoader::load(path);
        QVERIFY(loaded.isValid());
        QCOMPARE(loaded.width,       orig.width);
        QCOMPARE(loaded.height,      orig.height);
        QCOMPARE(loaded.name,        orig.name);
        QCOMPARE(loaded.tileSetName, orig.tileSetName);
        QCOMPARE(loaded.id,          orig.id);
        for (size_t i = 0; i < orig.tiles.size(); ++i)
            QCOMPARE(int(loaded.tiles[i]), int(orig.tiles[i]));
    }

    // -----------------------------------------------------------------------
    void testSaveNpmVerified()
    {
        Map orig;
        orig.width  = 5;
        orig.height = 5;
        orig.tiles.assign(25, 0);
        orig.tiles[12] = 999; // centre tile
        orig.name = "VerifiedMap";

        static QTemporaryDir tmpDir;
        // saveNpmVerified writes destPath.new first, then optionally replaces
        const QString dest = tmpDir.filePath("verified.npm");

        // Write original first so "replace" has something to replace
        QVERIFY(MapLoader::saveNpm(dest, orig));

        // Modify a tile, then save + verify + replace
        orig.tiles[0] = 42;
        QVERIFY(MapLoader::saveNpmVerified(dest, orig, true));

        const Map loaded = MapLoader::load(dest);
        QVERIFY(loaded.isValid());
        QCOMPARE(int(loaded.tiles[0]),  42);
        QCOMPARE(int(loaded.tiles[12]), 999);
    }

    // -----------------------------------------------------------------------
    void testOptParsing()
    {
        const QString optText =
            "ObjectiveCount: 2\n"
            "\n"
            "Name: Outpost#1\n"
            "Location: 10 20\n"
            "\n"
            "Name: Alpha Base\n"
            "Location: 55 77\n";
        const QString path = writeTempFile(optText.toUtf8(), ".opt");

        // Create a dummy .npm with matching base name
        Map m;
        m.width  = 128;
        m.height = 128;
        m.tiles.assign(128 * 128, 0);
        // Write the npm to the same directory
        static QTemporaryDir tmpDir;
        const QString npmPath = path.chopped(4) + ".npm";  // replace .opt→.npm
        const QByteArray npm = makeNpm(128, 128, {});
        QFile nf(npmPath); nf.open(QIODevice::WriteOnly); nf.write(npm);

        const Map loaded = MapLoader::load(npmPath);
        QVERIFY(loaded.isValid());

        int outposts = 0;
        for (const auto& obj : loaded.objects)
            if (obj.type == "outpost") ++outposts;
        QCOMPARE(outposts, 2);

        // Check names and coordinates
        const ObjectRef& o1 = loaded.objects[0];
        QCOMPARE(o1.name, QString("Outpost#1"));
        QCOMPARE(o1.x, 10);
        QCOMPARE(o1.y, 20);

        const ObjectRef& o2 = loaded.objects[1];
        QCOMPARE(o2.name, QString("Alpha Base"));
        QCOMPARE(o2.x, 55);
        QCOMPARE(o2.y, 77);
    }

    // -----------------------------------------------------------------------
    void testSpnParsing()
    {
        const QString spnText =
            "SpawnCount: 3\n"
            "Location: 5 10\n"
            "Location: 30 40\n"
            "Location: 99 7\n";
        const QString path = writeTempFile(spnText.toUtf8(), ".spn");

        const QString npmPath = path.chopped(4) + ".npm";
        const QByteArray npm = makeNpm(128, 128, {});
        QFile nf(npmPath); nf.open(QIODevice::WriteOnly); nf.write(npm);

        const Map loaded = MapLoader::load(npmPath);
        QVERIFY(loaded.isValid());

        int spawns = 0;
        for (const auto& obj : loaded.objects)
            if (obj.type == "spawnpoint") ++spawns;
        QCOMPARE(spawns, 3);

        const ObjectRef& s1 = loaded.objects[0];
        QCOMPARE(s1.x, 5);
        QCOMPARE(s1.y, 10);
        const ObjectRef& s3 = loaded.objects[2];
        QCOMPARE(s3.x, 99);
        QCOMPARE(s3.y, 7);
    }

    // -----------------------------------------------------------------------
    void testSpnParsing_noNewlines()
    {
        // Some real-world .spn files concatenate all tokens on one line.
        const QString spnText =
            "SpawnCount: 3Location: 5 10Location: 30 40Location: 99 7";
        const QString path = writeTempFile(spnText.toUtf8(), ".spn");

        const QString npmPath = path.chopped(4) + ".npm";
        const QByteArray npm = makeNpm(128, 128, {});
        QFile nf(npmPath); nf.open(QIODevice::WriteOnly); nf.write(npm);

        const Map loaded = MapLoader::load(npmPath);
        QVERIFY(loaded.isValid());

        int spawns = 0;
        for (const auto& obj : loaded.objects)
            if (obj.type == "spawnpoint") ++spawns;
        QCOMPARE(spawns, 3);

        QCOMPARE(loaded.objects[0].x, 5);
        QCOMPARE(loaded.objects[0].y, 10);
        QCOMPARE(loaded.objects[2].x, 99);
        QCOMPARE(loaded.objects[2].y, 7);
    }

    // -----------------------------------------------------------------------
    void testOptWrite_roundTrip()
    {
        Map orig;
        orig.width  = 16;
        orig.height = 16;
        orig.tiles.assign(256, 0);

        ObjectRef o1; o1.type = "outpost";    o1.name = "Alpha"; o1.x = 3;  o1.y = 7;
        ObjectRef o2; o2.type = "spawnpoint";                    o2.x = 11; o2.y = 2;
        orig.objects = {o1, o2};

        static QTemporaryDir tmpDir;
        const QString path = tmpDir.filePath("objects.npm");

        QVERIFY(MapLoader::saveNpm(path, orig));

        const Map loaded = MapLoader::load(path);
        QVERIFY(loaded.isValid());
        QCOMPARE((int)loaded.objects.size(), 2);

        const ObjectRef& lo1 = loaded.objects[0];
        QCOMPARE(lo1.type, QString("outpost"));
        QCOMPARE(lo1.name, QString("Alpha"));
        QCOMPARE(lo1.x, 3);
        QCOMPARE(lo1.y, 7);

        const ObjectRef& lo2 = loaded.objects[1];
        QCOMPARE(lo2.type, QString("spawnpoint"));
        QCOMPARE(lo2.x, 11);
        QCOMPARE(lo2.y, 2);
    }

    // -----------------------------------------------------------------------
    void testInvalidFile()
    {
        const Map m = MapLoader::load("/nonexistent/path/that/does/not/exist.npm");
        QVERIFY(!m.isValid());
    }
};

QTEST_MAIN(TestMapLoader)
#include "test_maploader.moc"
