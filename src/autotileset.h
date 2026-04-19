#pragma once
#include <QString>
#include <QHash>
#include <QSet>
#include <vector>
#include <cstdint>

// One terrain group: maps 8-neighbour bitmask -> tile ID.
// Bitmask bit layout (clockwise from top, blob convention):
//   bit 0 (  1): N    bit 1 (  2): NE   bit 2 (  4): E
//   bit 3 (  8): SE   bit 4 ( 16): S    bit 5 ( 32): SW
//   bit 6 ( 64): W    bit 7 (128): NW
// Diagonal bits are cleared unless both adjacent cardinal bits are also set.
struct AutotileGroup {
    QString      name;
    QHash<int,int> bitmask_to_tile; // bitmask -> tile ID
    QSet<int>    member_tiles;      // all tile IDs that belong to this group
};

// Loads a per-tileset JSON sidecar describing autotile terrain groups.
// Sidecar filename convention: <tileset_stem>.autotile.json
//
// JSON schema:
//   { "groups": [ { "name": "grass", "bitmask": { "0": 10, "4": 11, … } } ] }
//
// Keys in "bitmask" are decimal strings (0-255); values are tile IDs.
class AutotileSet {
public:
    bool load(const QString& path);
    void clear();
    bool isLoaded() const { return !m_groups.empty(); }

    // Returns the group containing tileId, or nullptr if not in any group.
    const AutotileGroup* groupForTile(int tileId) const;

    // Compute the 8-neighbour bitmask for the tile at (tx, ty).
    static int computeBitmask(const uint16_t* tiles, int mapW, int mapH,
                               int tx, int ty, const AutotileGroup& grp);

    // Look up the tile for a bitmask; falls back to bitmask 0 if not found.
    // Returns -1 if neither the exact bitmask nor 0 is in the map.
    static int tileForBitmask(const AutotileGroup& grp, int bitmask);

private:
    std::vector<AutotileGroup> m_groups;
    QHash<int,int>             m_tileToGroup; // tile ID -> index into m_groups
};
