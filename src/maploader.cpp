#include "maploader.h"
#include "npmformat.h"
#include <QFile>
#include <QByteArray>
#include <QTextStream>
#include <QFileInfo>
#include <algorithm>
#include <cstring>

// ---------------------------------------------------------------------------
// Helpers: read/write little-endian uint16

static uint16_t readU16(const unsigned char* buf, int offset)
{
    return uint16_t(buf[offset]) | (uint16_t(buf[offset + 1]) << 8);
}

static void writeU16(QByteArray& buf, int offset, uint16_t value)
{
    buf[offset]     = char(value & 0xFF);
    buf[offset + 1] = char((value >> 8) & 0xFF);
}

// Write a null-padded fixed-length string field into buf
static void writeField(QByteArray& buf, int offset, int fieldLen,
                       const QString& str)
{
    QByteArray latin = str.toLatin1();
    int n = std::min(latin.size(), fieldLen - 1); // leave room for null
    for (int i = 0; i < n; ++i)
        buf[offset + i] = latin[i];
    for (int i = n; i < fieldLen; ++i)
        buf[offset + i] = '\0';
}

// Read a null-terminated string from a fixed-length field
static QString readField(const char* buf, int fieldLen)
{
    int len = 0;
    while (len < fieldLen && buf[len] != '\0')
        ++len;
    return QString::fromLatin1(buf, len);
}

// ---------------------------------------------------------------------------
// .opt parsing  (companion objectives / outpost file)

void MapLoader::loadOpt(const QString& path, Map& m)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream in(&f);

    QString pendingName;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("Name:"))
            pendingName = line.mid(5).trimmed();
        else if (line.startsWith("Location:")) {
            const QStringList parts = line.mid(9).trimmed().split(' ',
                                               Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                ObjectRef obj;
                obj.type = "outpost";
                obj.name = pendingName.isEmpty() ? QString("Outpost#%1").arg(m.objects.size() + 1)
                                                 : pendingName;
                obj.x = parts[0].toInt();
                obj.y = parts[1].toInt();
                m.objects.push_back(std::move(obj));
                pendingName.clear();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// .spn parsing  (spawn point file)

void MapLoader::loadSpn(const QString& path, Map& m)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream in(&f);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("Location:")) {
            const QStringList parts = line.mid(9).trimmed().split(' ',
                                               Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                ObjectRef obj;
                obj.type = "spawnpoint";
                obj.x    = parts[0].toInt();
                obj.y    = parts[1].toInt();
                m.objects.push_back(std::move(obj));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Binary .npm loader

Map MapLoader::loadNpm(const QByteArray& data, const QString& path)
{
    Map m;
    if (data.size() < NPM_MIN_SIZE)
        return m;

    const unsigned char* buf = reinterpret_cast<const unsigned char*>(data.constData());

    m.idHeader    = readField(data.constData() + NPM_OFF_ID_HEADER, NPM_HEADER_ID_SIZE);
    m.id          = readU16(buf, NPM_OFF_ID);
    m.name        = readField(data.constData() + NPM_OFF_NAME,        NPM_NAME_SIZE);
    m.description = readField(data.constData() + NPM_OFF_DESCRIPTION, NPM_DESC_SIZE);
    m.width       = int(readU16(buf, NPM_OFF_WIDTH));
    m.height      = int(readU16(buf, NPM_OFF_HEIGHT));
    m.tileSetName = readField(data.constData() + NPM_OFF_TILESET, NPM_TILESET_SIZE);
    m.thumbW      = readU16(buf, NPM_OFF_THUMB_W);
    m.thumbH      = readU16(buf, NPM_OFF_THUMB_H);

    if (m.width <= 0 || m.height <= 0 || m.width > 4096 || m.height > 4096)
        return Map{};

    const qint64 tileCount   = qint64(m.width) * m.height;
    const qint64 tileDataEnd = NPM_OFF_TILES + tileCount * 2;
    if (data.size() < tileDataEnd)
        return Map{};

    m.tiles.resize(size_t(tileCount));
    for (qint64 i = 0; i < tileCount; ++i)
        m.tiles[size_t(i)] = readU16(buf, int(NPM_OFF_TILES + i * 2));

    // Optional thumbnail
    const qint64 thumbBytes = qint64(m.thumbW) * m.thumbH;
    if (thumbBytes > 0 && data.size() >= tileDataEnd + thumbBytes)
        m.thumbnail = data.mid(int(tileDataEnd), int(thumbBytes));

    // Companion files
    const QString base = path.chopped(4); // remove ".npm"
    loadOpt(base + ".opt", m);
    loadSpn(base + ".spn", m);

    return m;
}

// ---------------------------------------------------------------------------
// Plain-text fallback loader:  "W H\n tile tile tile..."

Map MapLoader::loadText(const QByteArray& data)
{
    Map m;
    const QString text = QString::fromUtf8(data);
    QTextStream in(const_cast<QString*>(&text), QIODevice::ReadOnly);

    int w = 0, h = 0;
    in >> w >> h;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096)
        return m;

    m.width  = w;
    m.height = h;
    m.tiles.resize(size_t(w * h));

    for (int i = 0; i < w * h; ++i) {
        int v = 0;
        in >> v;
        m.tiles[size_t(i)] = uint16_t(v);
    }
    return m;
}

// ---------------------------------------------------------------------------
// Public load entry point

Map MapLoader::load(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return Map{};
    const QByteArray data = f.readAll();

    if (path.endsWith(".npm", Qt::CaseInsensitive))
        return loadNpm(data, path);

    return loadText(data);
}

// ---------------------------------------------------------------------------
// Save companions: .opt and .spn

static bool writeOptSpn(const QString& basePath, const Map& m)
{
    // .opt
    {
        QFile optf(basePath + ".opt");
        if (!optf.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        QTextStream out(&optf);
        int count = 0;
        for (const auto& obj : m.objects)
            if (obj.type == "outpost") ++count;
        out << "ObjectiveCount: " << count << "\n";
        int idx = 1;
        for (const auto& obj : m.objects) {
            if (obj.type != "outpost") continue;
            out << "\nName: " << (obj.name.isEmpty()
                                  ? QString("Outpost#%1").arg(idx) : obj.name) << "\n";
            out << "Location: " << obj.x << " " << obj.y << "\n";
            ++idx;
        }
    }

    // .spn
    {
        QFile spnf(basePath + ".spn");
        if (!spnf.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        QTextStream out(&spnf);
        int count = 0;
        for (const auto& obj : m.objects)
            if (obj.type == "spawnpoint") ++count;
        out << "SpawnCount: " << count << "\n";
        for (const auto& obj : m.objects) {
            if (obj.type != "spawnpoint") continue;
            out << "Location: " << obj.x << " " << obj.y << "\n";
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Text save

bool MapLoader::saveText(const QString& path, const Map& m)
{
    if (!m.isValid()) return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&f);
    out << m.width << " " << m.height << "\n";
    for (int y = 0; y < m.height; ++y) {
        for (int x = 0; x < m.width; ++x) {
            out << m.tiles[size_t(y * m.width + x)];
            if (x + 1 < m.width) out << ' ';
        }
        out << '\n';
    }
    f.close();

    // Write companion files next to the given path (strip known extension)
    QString base = path;
    if (base.endsWith(".npm", Qt::CaseInsensitive) ||
        base.endsWith(".txt", Qt::CaseInsensitive))
        base = base.chopped(4);
    return writeOptSpn(base, m);
}

// ---------------------------------------------------------------------------
// Binary .npm save (from scratch)

bool MapLoader::saveNpm(const QString& path, const Map& m)
{
    if (!m.isValid()) return false;

    const qint64 tileCount  = qint64(m.width) * m.height;
    const qint64 thumbBytes = qint64(m.thumbW) * m.thumbH;
    const qint64 totalSize  = NPM_OFF_TILES + tileCount * 2 + thumbBytes;

    QByteArray buf(int(totalSize), '\0');

    // id_header
    const QString hdr = m.idHeader.isEmpty()
                        ? "Copyright PyroSoft Inc." : m.idHeader;
    writeField(buf, NPM_OFF_ID_HEADER, NPM_HEADER_ID_SIZE, hdr);

    writeU16(buf, NPM_OFF_ID, m.id);
    writeField(buf, NPM_OFF_NAME,        NPM_NAME_SIZE,  m.name);
    writeField(buf, NPM_OFF_DESCRIPTION, NPM_DESC_SIZE,  m.description);
    writeU16(buf, NPM_OFF_WIDTH,  uint16_t(m.width));
    writeU16(buf, NPM_OFF_HEIGHT, uint16_t(m.height));
    writeField(buf, NPM_OFF_TILESET, NPM_TILESET_SIZE, m.tileSetName);
    writeU16(buf, NPM_OFF_THUMB_W, m.thumbW);
    writeU16(buf, NPM_OFF_THUMB_H, m.thumbH);

    // Tile data
    for (qint64 i = 0; i < tileCount; ++i)
        writeU16(buf, int(NPM_OFF_TILES + i * 2), m.tiles[size_t(i)]);

    // Thumbnail (preserved from load if available)
    if (thumbBytes > 0 && m.thumbnail.size() >= thumbBytes)
        std::memcpy(buf.data() + NPM_OFF_TILES + tileCount * 2,
                    m.thumbnail.constData(), size_t(thumbBytes));

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    if (f.write(buf) != totalSize)
        return false;
    f.close();

    // Companion files — strip .npm.new or .npm to get the base name
    QString base = path;
    if (base.endsWith(".npm.new", Qt::CaseInsensitive))
        base = base.chopped(8);
    else if (base.endsWith(".npm", Qt::CaseInsensitive))
        base = base.chopped(4);
    return writeOptSpn(base, m);
}

// ---------------------------------------------------------------------------
// Verified save

bool MapLoader::saveNpmVerified(const QString& destPath, const Map& m, bool replace)
{
    const QString newPath = destPath.endsWith(".npm", Qt::CaseInsensitive)
                            ? destPath + ".new"
                            : destPath;

    if (!saveNpm(newPath, m))
        return false;

    // Reload and verify tile array.
    // We force binary (.npm) parsing regardless of the ".new" extension by reading
    // the data ourselves and calling loadNpm directly.
    QFile vf(newPath);
    if (!vf.open(QIODevice::ReadOnly)) return false;
    const Map reloaded = loadNpm(vf.readAll(), destPath); // use destPath as hint
    vf.close(); // must close before rename on Windows
    if (!reloaded.isValid()) return false;
    if (reloaded.width != m.width || reloaded.height != m.height) return false;
    if (reloaded.tiles.size() != m.tiles.size()) return false;
    for (size_t i = 0; i < m.tiles.size(); ++i)
        if (reloaded.tiles[i] != m.tiles[i]) return false;

    if (replace) {
        const QString backup = destPath + ".bak";
        QFile::remove(backup);
        if (!QFile::rename(destPath, backup)) return false;
        if (!QFile::rename(newPath, destPath)) {
            QFile::rename(backup, destPath); // try to restore
            return false;
        }
    }
    return true;
}
