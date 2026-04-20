#pragma once
#include <QString>
#include <QHash>
#include <QSet>
#include <QPair>
#include <vector>
#include <cstdint>

// One terrain group: maps bitmask -> tile ID (blob8) or piece (4dir).
//
// blob8 bitmask bit layout (clockwise from top):
//   bit 0 (  1): N    bit 1 (  2): NE   bit 2 (  4): E
//   bit 3 (  8): SE   bit 4 ( 16): S    bit 5 ( 32): SW
//   bit 6 ( 64): W    bit 7 (128): NW
//   Diagonal bits cleared unless both adjacent cardinal bits are set.
//
// 4dir bitmask bit layout:
//   bit 0 (1): N   bit 1 (2): E   bit 2 (4): S   bit 3 (8): W
struct AutotileGroup {
    QString name;
    bool    is4dir     = false;
    int     piece_size = 1;        // >1 enables multi-tile piece mode

    // ---- blob8 (piece_size == 1) ----
    QHash<int,int> bitmask_to_tile; // bitmask -> tile ID

    // ---- 4dir (piece_size > 1) ----
    struct Piece {
        int              w = 0, h = 0;
        std::vector<int> tiles; // row-major [row*w + col], -1 = empty
    };
    QHash<int, Piece>          bitmask_to_piece;      // 4dir bitmask -> piece
    QHash<int, QPair<int,int>> tile_to_offset;         // tile_id -> (dx, dy) within piece
    QHash<int, int>            tile_to_piece_bitmask;  // tile_id -> its piece's bitmask

    // ---- shared ----
    QSet<int> member_tiles; // all tile IDs belonging to this group
};

// Loads a per-tileset JSON sidecar describing autotile terrain groups.
// Sidecar filename convention: <tileset_stem>.autotile.json
//
// blob8 JSON schema:
//   { "groups": [ { "name": "grass", "bitmask": { "0": 10, "4": 11, … } } ] }
//
// 4dir JSON schema (piece_size > 1):
//   { "groups": [ {
//       "name": "wall", "type": "4dir", "piece_size": 3,
//       "member_tiles": [...],
//       "pieces": {
//         "10": [[tl,tm,tr],[ml,mm,mr],[bl,bm,br]],
//         "5":  [[...],[...],[...]]
//       }
//   } ] }
//   Keys in "pieces" are decimal 4dir bitmask strings (0-15).
class AutotileSet {
public:
    bool load(const QString& path);
    void clear();
    bool isLoaded() const { return !m_groups.empty(); }

    // Returns the group containing tileId, or nullptr.
    const AutotileGroup* groupForTile(int tileId) const;

    // ---- blob8 ----
    // Compute the 8-neighbour bitmask for the tile at (tx, ty).
    static int computeBitmask(const uint16_t* tiles, int mapW, int mapH,
                               int tx, int ty, const AutotileGroup& grp);

    // Tile for a blob8 bitmask; falls back to bitmask 255, then -1.
    static int tileForBitmask(const AutotileGroup& grp, int bitmask);

    // ---- 4dir piece ----
    // Compute the 4-dir bitmask for the piece whose top-left is at (px, py).
    static int computePieceBitmask(const uint16_t* tiles, int mapW, int mapH,
                                    int px, int py, int piece_size,
                                    const AutotileGroup& grp);

    // Piece for a 4dir bitmask.
    // intent_bitmask: the piece bitmask of the originally selected tile, used
    // as a final fallback when the computed bitmask has no exact match and no
    // axis-based fallback applies (e.g. isolated placement, bitmask 0).
    static const AutotileGroup::Piece* pieceForBitmask(const AutotileGroup& grp,
                                                        int bitmask,
                                                        int intent_bitmask = 15);

private:
    std::vector<AutotileGroup> m_groups;
    QHash<int,int>             m_tileToGroup; // tile ID -> index into m_groups
};
