#pragma once
#include <QDockWidget>
#include <QWidget>
#include <QPixmap>
#include "tlsloader.h"

// Internal scrollable widget that renders the tile grid.
class TilePanelWidget : public QWidget {
    Q_OBJECT
public:
    static constexpr int RENDER_SIZE = 32; // pixels per tile in the panel

    explicit TilePanelWidget(QWidget* parent = nullptr);

    void setTileset(const Tileset* ts);
    const Tileset* tileset() const { return m_tileset; }

    int selectedTile() const { return m_selectedTile; }

signals:
    void tileSelected(int id);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    QSize sizeHint() const override;

private:
    int cols() const;
    int tileAt(QPoint pos) const;

    const Tileset* m_tileset = nullptr;
    QPixmap        m_atlasPixmap;
    static constexpr int ATLAS_COLS = 64;

    int m_selectedTile = 0;
    int m_hoveredTile  = -1;
};


// Dock widget wrapping TilePanelWidget inside a QScrollArea.
class TilePanel : public QDockWidget {
    Q_OBJECT
public:
    explicit TilePanel(QWidget* parent = nullptr);

    void setTileset(const Tileset* ts);
    TilePanelWidget* panelWidget() const { return m_widget; }

signals:
    void tileSelected(int id);

private:
    TilePanelWidget* m_widget;
};
