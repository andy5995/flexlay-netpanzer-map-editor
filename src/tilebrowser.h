#pragma once
#include <QDockWidget>
#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QScrollArea>
#include "tlsloader.h"
#include "stamp.h"

class TileBrowserWidget : public QWidget {
    Q_OBJECT
public:
    explicit TileBrowserWidget(QWidget* parent = nullptr);

    void setTileset(const Tileset* ts);
    void setTileSize(int px);
    int  tileSize() const { return m_tileSize; }

signals:
    void tileSelected(int id);
    void stampCreated(Stamp s);   // emitted when a multi-tile drag-select is released

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    QSize sizeHint() const override;

private:
    int    cols() const;
    QPoint gridPos(QPoint widgetPos) const;   // (col, row) in tile grid
    int    tileIdAt(QPoint grid) const;       // tile id from grid pos, -1 if OOB

    const Tileset* m_tileset     = nullptr;
    QPixmap        m_atlasPixmap;
    static constexpr int ATLAS_COLS = 64;
    int m_tileSize     = 64;
    int m_selectedTile = 0;
    int m_hoveredTile  = -1;

    // Drag selection
    bool   m_dragging  = false;
    QPoint m_selStart;   // grid (col, row) where drag began
    QPoint m_selEnd;     // grid (col, row) at current drag position
};

class TileBrowser : public QDockWidget {
    Q_OBJECT
public:
    explicit TileBrowser(QWidget* parent = nullptr);

    void setTileset(const Tileset* ts);

signals:
    void tileSelected(int id);
    void stampCreated(Stamp s);

private:
    TileBrowserWidget* m_widget;
    QScrollArea*       m_scroll;
};
