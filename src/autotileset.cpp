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
        grp.name       = go.value("name").toString();
        grp.is4dir     = (go.value("type").toString() == "4dir");
        grp.piece_size = go.value("piece_size").toInt(1);

        // Always parse member_tiles from JSON
        for (const QJsonValue& mv : go.value("member_tiles").toArray())
            grp.member_tiles.insert(mv.toInt(-1));
        grp.member_tiles.remove(-1);

        if (grp.is4dir && grp.piece_size > 1) {
            const QJsonObject pieces = go.value("pieces").toObject();
            for (auto pit = pieces.constBegin(); pit != pieces.constEnd(); ++pit) {
                const int bm = pit.key().toInt();
                const QJsonArray rows = pit.value().toArray();
                AutotileGroup::Piece piece;
                piece.h = rows.size();
                piece.w = piece.h > 0 ? rows[0].toArray().size() : 0;
                piece.tiles.reserve(size_t(piece.w * piece.h));
                for (int r = 0; r < piece.h; ++r) {
                    const QJsonArray row = rows[r].toArray();
                    for (int c = 0; c < piece.w; ++c) {
                        const int tid = row[c].toInt(-1);
                        piece.tiles.push_back(tid);
                        if (tid >= 0) {
                            grp.member_tiles.insert(tid);
                            grp.tile_to_offset[tid]        = {c, r};
                            grp.tile_to_piece_bitmask[tid] = bm;
                        }
                    }
                }
                grp.bitmask_to_piece[bm] = std::move(piece);
            }
        } else {
            const QJsonObject bm = go.value("bitmask").toObject();
            for (auto it = bm.constBegin(); it != bm.constEnd(); ++it) {
                const int mask = it.key().toInt();
                const int tid  = it.value().toInt(-1);
                if (tid < 0) continue;
                grp.bitmask_to_tile[mask] = tid;
                grp.member_tiles.insert(tid);
            }
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

// ---------------------------------------------------------------------------
// blob8

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
    it = grp.bitmask_to_tile.constFind(255);
    if (it != grp.bitmask_to_tile.constEnd()) return it.value();
    return -1;
}

// ---------------------------------------------------------------------------
// 4dir piece

int AutotileSet::computePieceBitmask(const uint16_t* tiles, int mapW, int mapH,
                                      int px, int py, int piece_size,
                                      const AutotileGroup& grp)
{
    // Check whether any member tile exists along an edge adjacent to this piece.
    auto edgeHasMember = [&](int x0, int y0, int x1, int y1) -> bool {
        for (int y = y0; y <= y1; ++y) {
            if (y < 0 || y >= mapH) continue;
            for (int x = x0; x <= x1; ++x) {
                if (x < 0 || x >= mapW) continue;
                if (grp.member_tiles.contains(int(tiles[y * mapW + x])))
                    return true;
            }
        }
        return false;
    };

    const int n = edgeHasMember(px, py - 1, px + piece_size - 1, py - 1)             ? 1 : 0;
    const int e = edgeHasMember(px + piece_size, py, px + piece_size, py + piece_size - 1) ? 2 : 0;
    const int s = edgeHasMember(px, py + piece_size, px + piece_size - 1, py + piece_size) ? 4 : 0;
    const int w = edgeHasMember(px - 1, py, px - 1, py + piece_size - 1)             ? 8 : 0;
    return n | e | s | w;
}

const AutotileGroup::Piece* AutotileSet::pieceForBitmask(const AutotileGroup& grp,
                                                           int bitmask,
                                                           int intent_bitmask)
{
    auto find = [&](int bm) -> const AutotileGroup::Piece* {
        auto it = grp.bitmask_to_piece.constFind(bm);
        return (it != grp.bitmask_to_piece.constEnd()) ? &it.value() : nullptr;
    };

    if (auto* p = find(bitmask)) return p;

    // Axis-aware fallback: pure N/S → vertical (5); pure E/W → horizontal (10).
    const bool hasN = bitmask & 1, hasE = bitmask & 2, hasS = bitmask & 4, hasW = bitmask & 8;
    if ((hasN || hasS) && !hasE && !hasW) { if (auto* p = find(5))  return p; }
    if ((hasE || hasW) && !hasN && !hasS) { if (auto* p = find(10)) return p; }

    // Intent fallback: use the piece type the user was painting with.
    if (auto* p = find(intent_bitmask)) return p;

    // Last resort: cross (15).
    return find(15);
}
