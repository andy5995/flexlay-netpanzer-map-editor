#include "autotileset.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

bool AutotileSet::load(const QString& path)
{
    clear();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (doc.isNull() || !doc.isObject()) return false;

    const QJsonArray groups = doc.object().value("groups").toArray();
    m_groups.reserve(size_t(groups.size()));

    for (const QJsonValue& gv : groups) {
        const QJsonObject go = gv.toObject();
        AutotileGroup grp;
        grp.name = go.value("name").toString();

        for (const QJsonValue& mv : go.value("member_tiles").toArray())
            grp.member_tiles.insert(mv.toInt(-1));
        grp.member_tiles.remove(-1);

        const QJsonObject bm = go.value("bitmask").toObject();
        for (auto it = bm.constBegin(); it != bm.constEnd(); ++it) {
            const int mask = it.key().toInt();
            const int tid  = it.value().toInt(-1);
            if (tid < 0) continue;
            grp.bitmask_to_tile[mask] = tid;
            grp.member_tiles.insert(tid);
        }

        if (grp.member_tiles.isEmpty()) continue;

        const int gidx = int(m_groups.size());
        for (int tid : grp.member_tiles)
            m_tileToGroup[tid] = gidx;

        m_groups.push_back(std::move(grp));
    }
    return !m_groups.empty();
}

void AutotileSet::clear()
{
    m_groups.clear();
    m_tileToGroup.clear();
}

const AutotileGroup* AutotileSet::groupForTile(int tileId) const
{
    const auto it = m_tileToGroup.constFind(tileId);
    if (it == m_tileToGroup.constEnd()) return nullptr;
    return &m_groups[size_t(it.value())];
}

int AutotileSet::computeBitmask(const uint16_t* tiles, int mapW, int mapH,
                                 int tx, int ty, const AutotileGroup& grp)
{
    auto inGrp = [&](int x, int y) -> bool {
        if (x < 0 || x >= mapW || y < 0 || y >= mapH) return false;
        return grp.member_tiles.contains(int(tiles[y * mapW + x]));
    };

    const bool n  = inGrp(tx,   ty-1);
    const bool e  = inGrp(tx+1, ty);
    const bool s  = inGrp(tx,   ty+1);
    const bool w  = inGrp(tx-1, ty);
    const bool ne = inGrp(tx+1, ty-1) && n && e;
    const bool se = inGrp(tx+1, ty+1) && s && e;
    const bool sw = inGrp(tx-1, ty+1) && s && w;
    const bool nw = inGrp(tx-1, ty-1) && n && w;

    return (n  ?   1 : 0)
         | (ne ?   2 : 0)
         | (e  ?   4 : 0)
         | (se ?   8 : 0)
         | (s  ?  16 : 0)
         | (sw ?  32 : 0)
         | (w  ?  64 : 0)
         | (nw ? 128 : 0);
}

int AutotileSet::tileForBitmask(const AutotileGroup& grp, int bitmask)
{
    auto it = grp.bitmask_to_tile.constFind(bitmask);
    if (it != grp.bitmask_to_tile.constEnd()) return it.value();
    // Fall back to fully-surrounded interior tile (bitmask 255).
    it = grp.bitmask_to_tile.constFind(255);
    if (it != grp.bitmask_to_tile.constEnd()) return it.value();
    return -1;
}
