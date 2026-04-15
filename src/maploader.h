#pragma once
#include "objects.h"

class QString;
class QByteArray;

class MapLoader {
public:
    // Load from .npm binary or plain text format.
    // Companion .opt/.spn files are loaded automatically when a .npm is opened.
    static Map load(const QString& path);

    // Save as a plain-text format (W H on first line, tile values following).
    // Companion .opt and .spn files are written beside the given path.
    static bool saveText(const QString& path, const Map& m);

    // Write a binary .npm from scratch (not a patcher).
    // Missing metadata fields are filled with defaults.
    static bool saveNpm(const QString& path, const Map& m);

    // Write .npm, reload and verify tiles match, then optionally replace the
    // original (with a .bak backup). Returns false on any failure.
    static bool saveNpmVerified(const QString& destPath, const Map& m, bool replace = false);

private:
    static Map  loadNpm(const QByteArray& data, const QString& path);
    static Map  loadText(const QByteArray& data);
    static void loadOpt(const QString& path, Map& m);
    static void loadSpn(const QString& path, Map& m);
};
