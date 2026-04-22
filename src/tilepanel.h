#pragma once
#include <QDockWidget>
#include <QWidget>
#include <QPixmap>
#include <QScrollArea>
#include "tlsloader.h"
#include "stamp.h"

// Internal scrollable widget that renders the tile grid.
class TilePanelWidget : public QWidget {
    Q_OBJECT
public:
    static constexpr int RENDER_SIZE = 32; // pixels per tile in the panel

    explicit TilePanelWidget(QWidget* parent = nullptr);

    void setTileset(const Tileset* ts);
    const Tileset* tileset() const { return m_tileset; }

    int selectedTile() const { return m_selectedTile; }
    void setSelectedTile(int id);

    int cols() const;

signals:
    void tileSelected(int id);
    void stampCreated(Stamp s);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void keyPressEvent(QKeyEvent* ev) override;
    QSize sizeHint() const override;

private:
    QPoint gridPos(QPoint widgetPos) const;
    int    tileAt(QPoint pos) const;
    int    tileIdAtGrid(int col, int row) const;

    const Tileset* m_tileset = nullptr;
    QPixmap        m_atlasPixmap;
    static constexpr int ATLAS_COLS = 64;

    int m_selectedTile = 0;
    int m_hoveredTile  = -1;

    bool   m_dragging = false;
    QPoint m_selStart;
    QPoint m_selEnd;
};


// Dock widget wrapping TilePanelWidget inside a QScrollArea.
class TilePanel : public QDockWidget {
    Q_OBJECT
public:
    explicit TilePanel(QWidget* parent = nullptr);

    void setTileset(const Tileset* ts);
    void setSelectedTile(int id);
    TilePanelWidget* panelWidget() const { return m_widget; }

signals:
    void tileSelected(int id);
    void stampCreated(Stamp s);

private:
    TilePanelWidget* m_widget;
    QScrollArea*     m_scroll;
};
