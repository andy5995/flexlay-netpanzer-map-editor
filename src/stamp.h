#pragma once
#include <QString>
#include <vector>
#include <cstdint>

struct Stamp {
    QString name;
    int width  = 0;
    int height = 0;
    std::vector<uint16_t> tiles; // row-major, width*height entries
};
