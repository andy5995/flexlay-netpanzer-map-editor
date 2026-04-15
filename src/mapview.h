#pragma once
#include <QWidget>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QPixmap>
#include <QSet>
#include <vector>
#include <memory>
#include "objects.h"
#include "tlsloader.h"
#include "commands.h"

enum class Tool {
    TilePaint,       // left-drag paints selected tile
    PlaceOutpost,    // left-click places an outpost
    PlaceSpawnpoint, // left-click places a spawn point
    SelectObject     // left-click selects/drags objects; Del removes
};

class MapView : public QWidget {
    Q_OBJECT
public:
    static constexpr int TILE_SIZE = 32;

    explicit MapView(QWidget* parent = nullptr);

    void setMap(const Map& map);
    const Map& map() const { return m_map; }

    void setTileset(const Tileset* ts);
    const Tileset* tileset() const { return m_tileset; }

    // Zoom / pan
    void setZoom(double z);
    double zoom() const { return m_zoom; }
    void   fitToWindow();

    // Tool
    void setTool(Tool t);
    Tool tool() const { return m_tool; }

    // Tile painting
    void setSelectedTile(int id) { m_selectedTile = id; }
    int  selectedTile()   const  { return m_selectedTile; }

    // View options
    void setShowGrid(bool show) { m_showGrid = show; update(); }
    bool showGrid()       const  { return m_showGrid; }

    // Object access
    int  selectedObject() const { return m_selectedObj; }
    void deleteSelectedObject();

    // Apply a command to the map (apply + push to undo stack).
    // For use by external code (e.g. MainWindow rename dialog).
    void applyCommand(std::unique_ptr<Command> cmd);

    // Pan so that the given tile coordinate is centred in the viewport.
    void panToTile(QPointF tilePt);

    // Undo / redo
    void undo();
    void redo();
    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }

signals:
    void tileHovered(int tileX, int tileY, int tileId);
    void mapModified();
    void objectSelectionChanged(int idx); // -1 = none
    void objectActivated(int idx);        // double-click on an object
    void viewportChanged(QRectF tileRect);

protected:
    void paintEvent(QPaintEvent*)         override;
    void wheelEvent(QWheelEvent*)         override;
    void mousePressEvent(QMouseEvent*)    override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*)     override;
    void mouseReleaseEvent(QMouseEvent*)  override;
    void leaveEvent(QEvent*)              override;
    void keyPressEvent(QKeyEvent*)        override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void resizeEvent(QResizeEvent*)       override;

private:
    // Coordinate mapping
    bool widgetToTile(QPoint widgetPos, int& tx, int& ty) const;
    QPointF widgetToMapPx(QPoint widgetPos) const;

    // Object hit test (returns index or -1)
    int objectAt(QPoint widgetPos) const;

    // Tile paint helpers
    void startStroke();
    void addToStroke(int tx, int ty);
    void commitStroke();

    // Command stack (unified for tiles and objects)
    // pushCommand: command already applied to m_map (tile batches)
    void pushCommand(std::unique_ptr<Command> cmd);

    void emitViewportChanged();

    // Map / rendering
    Map            m_map;
    double         m_zoom     = 1.0;
    QPoint         m_pan;
    bool           m_showGrid = true;

    const Tileset* m_tileset  = nullptr;
    QPixmap        m_atlasPixmap;
    static constexpr int ATLAS_COLS = 64;

    // Tool state
    Tool m_tool        = Tool::TilePaint;
    int  m_selectedTile = 0;
    int  m_selectedObj  = -1;

    // Pan state (middle button)
    bool   m_panning   = false;
    QPoint m_lastMouse;

    // Tile stroke state
    std::unique_ptr<TileBatch> m_currentStroke;
    QSet<int>                  m_strokeTiles; // indices painted this stroke

    // Object drag state
    bool m_draggingObj  = false;
    int  m_objDragOrigX = 0;
    int  m_objDragOrigY = 0;

    // Command stacks
    std::vector<std::unique_ptr<Command>> m_undo;
    std::vector<std::unique_ptr<Command>> m_redo;
};
