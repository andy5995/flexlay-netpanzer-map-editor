#pragma once
#include <vector>
#include <cstdint>
#include <QString>
#include <QByteArray>

struct ObjectRef {
    QString type;  // "outpost" or "spawnpoint"
    QString name;  // e.g. "Outpost#1" (meaningful for outposts)
    int x = 0;     // tile coordinates (not pixels)
    int y = 0;
};

struct Map {
    int width = 0;
    int height = 0;
    std::vector<uint16_t> tiles;    // tile indices into the tileset
    std::vector<ObjectRef> objects;

    // Metadata from .npm header — preserved on load, written on save
    QString idHeader;       // raw 64-byte id string (e.g. "Copyright PyroSoft Inc.")
    uint16_t id = 1;
    QString name;           // map display name
    QString description;
    QString tileSetName;    // e.g. "summer12mb.tls"
    uint16_t thumbW = 0;
    uint16_t thumbH = 0;
    QByteArray thumbnail;   // raw palette-indexed thumbnail bytes (may be empty)

    bool isValid() const { return width > 0 && height > 0 &&
                                  (int)tiles.size() == width * height; }
};
