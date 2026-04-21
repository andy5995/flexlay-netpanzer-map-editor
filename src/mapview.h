#pragma once
#include <QWidget>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QPixmap>
#include <QSet>
#include <vector>
#include <memory>
#include "objects.h"
#include "tlsloader.h"
#include "commands.h"
#include "stamp.h"
#include "autotileset.h"

enum class Tool {
    TilePaint,       // left-drag paints selected tile
    EllipsePaint,    // drag to paint selected tile along ellipse outline
    RectOutline,     // drag to paint selected tile along rectangle outline
    TilePick,        // left-click picks tile under cursor, switches to TilePaint
    RectSelect,      // drag to select a rectangular tile region
    RectFill,        // drag to fill a rectangular tile region
    StampPaint,      // click to place the current stamp
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
    Map&       map()       { return m_map; }

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

    // Autotiling
    void setAutotileSet(AutotileSet ats) { m_autotileSet = std::move(ats); }
    void setAutotileEnabled(bool en)     { m_autotileEnabled = en; }
    bool autotileEnabled() const         { return m_autotileEnabled; }

    // Rect selection
    QRect selection() const { return m_selection; }
    bool  hasSelection() const { return !m_selection.isNull(); }
    Stamp captureSelection() const;

    // Stamp painting
    void setCurrentStamp(const Stamp* stamp);

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
    void tilePicked(int tileId);        // emitted when TilePick tool clicks a tile
    void toolChanged(Tool t);           // emitted when tool changes internally (e.g. pick→paint)
    void selectionChanged(QRect sel);   // emitted when rect selection changes
    void mapModified();
    void objectSelectionChanged(int idx); // -1 = none
    void objectActivated(int idx);        // double-click on an object
    void viewportChanged(QRectF tileRect);
    void stampDeselected();               // right-click or Escape in stamp mode

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
    void updateAutotileNeighbors(int tx, int ty, const AutotileGroup& grp);

    // 4dir piece helpers
    void addPieceToStroke(int px, int py, const AutotileGroup& grp);
    void updatePieceNeighbors(int px, int py, const AutotileGroup& grp);
    void applyPieceTiles(int px, int py, const AutotileGroup::Piece& piece);

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

    // Autotile state
    AutotileSet m_autotileSet;
    bool        m_autotileEnabled = false;

    // Rect selection state
    QRect  m_selection;          // in tile coords, null if none
    bool   m_selecting  = false;
    QPoint m_selectStart;        // tile coord where drag began

    // Rect fill state
    bool   m_rectFilling  = false;
    QPoint m_rectFillStart;
    QRect  m_rectFillPreview;

    // Stamp paint state
    const Stamp* m_currentStamp    = nullptr;
    QPoint       m_stampHoverTile  = QPoint(-1, -1);

    // Outpost placement hover (tile coords, (-1,-1) = none)
    QPoint m_outpostHoverTile = QPoint(-1, -1);

    // Ellipse paint state
    QPoint m_ellipseStart;
    QPoint m_ellipseEnd;
    bool   m_ellipseActive = false;

    // Rect outline paint state
    QPoint m_rectOutlineStart;
    QPoint m_rectOutlineEnd;
    bool   m_rectOutlineActive = false;

    static std::vector<QPoint> computeEllipseTiles(QPoint a, QPoint b, int mapW, int mapH);
    static std::vector<QPoint> computeRectOutlineTiles(QPoint a, QPoint b, int mapW, int mapH);

    // Pan state (middle button)
    bool   m_panning   = false;
    QPoint m_lastMouse;

    void applyStamp(int tx, int ty);

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
