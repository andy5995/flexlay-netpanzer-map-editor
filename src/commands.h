#pragma once
#include "objects.h"
#include <vector>
#include <memory>
#include <algorithm>

// ---------------------------------------------------------------------------
// Abstract edit command.  Every undoable operation is represented as a Command
// that can apply() (do/redo) and revert() (undo) a change to a Map.

struct Command {
    virtual ~Command() = default;
    virtual void apply(Map& m)  = 0;
    virtual void revert(Map& m) = 0;
};

// ---------------------------------------------------------------------------
// A batch of tile edits accumulated during a single paint stroke.

struct TileBatch : Command {
    struct Edit { int idx; uint16_t oldVal; uint16_t newVal; };
    std::vector<Edit> edits;

    bool empty() const { return edits.empty(); }

    void apply(Map& m) override {
        for (const auto& e : edits)
            m.tiles[size_t(e.idx)] = e.newVal;
    }
    void revert(Map& m) override {
        for (auto it = edits.rbegin(); it != edits.rend(); ++it)
            m.tiles[size_t(it->idx)] = it->oldVal;
    }
};

// ---------------------------------------------------------------------------
// Object commands

struct AddObject : Command {
    ObjectRef obj;
    explicit AddObject(ObjectRef o) : obj(std::move(o)) {}
    void apply(Map& m)  override { m.objects.push_back(obj); }
    void revert(Map& m) override { if (!m.objects.empty()) m.objects.pop_back(); }
};

struct RemoveObject : Command {
    int       idx;
    ObjectRef obj;
    RemoveObject(int i, ObjectRef o) : idx(i), obj(std::move(o)) {}
    void apply(Map& m) override {
        if (idx >= 0 && idx < static_cast<int>(m.objects.size()))
            m.objects.erase(m.objects.begin() + idx);
    }
    void revert(Map& m) override {
        const int i = std::min(idx, static_cast<int>(m.objects.size()));
        m.objects.insert(m.objects.begin() + i, obj);
    }
};

struct MoveObject : Command {
    int idx;
    int oldX, oldY, newX, newY;
    MoveObject(int i, int ox, int oy, int nx, int ny)
        : idx(i), oldX(ox), oldY(oy), newX(nx), newY(ny) {}
    void apply(Map& m) override {
        if (idx >= 0 && idx < static_cast<int>(m.objects.size())) {
            m.objects[idx].x = newX;
            m.objects[idx].y = newY;
        }
    }
    void revert(Map& m) override {
        if (idx >= 0 && idx < static_cast<int>(m.objects.size())) {
            m.objects[idx].x = oldX;
            m.objects[idx].y = oldY;
        }
    }
};

struct RenameObject : Command {
    int     idx;
    QString oldName, newName;
    RenameObject(int i, QString on, QString nn)
        : idx(i), oldName(std::move(on)), newName(std::move(nn)) {}
    void apply(Map& m) override {
        if (idx >= 0 && idx < static_cast<int>(m.objects.size()))
            m.objects[idx].name = newName;
    }
    void revert(Map& m) override {
        if (idx >= 0 && idx < static_cast<int>(m.objects.size()))
            m.objects[idx].name = oldName;
    }
};
