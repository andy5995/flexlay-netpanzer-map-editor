#pragma once
#include <QDockWidget>
#include <QWidget>
#include <QImage>
#include <QRectF>
#include "objects.h"
#include "tlsloader.h"

// Internal widget: renders a 1-pixel-per-tile overview of the map.
// Uses the tileset's avg_color palette entry per tile for fast rendering,
// falling back to HSV-from-id when no tileset is loaded.
class MinimapView : public QWidget {
    Q_OBJECT
public:
    explicit MinimapView(QWidget* parent = nullptr);

    void setMap(const Map* m);
    void setTileset(const Tileset* ts);
    void setViewportRect(const QRectF& tileRect); // visible tile rect from MapView
    void rebuildImage(); // rebuild the cached QImage (call after tile edits)

signals:
    void panRequested(QPointF tilePt); // user clicked; centre view on this tile

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    QSize sizeHint() const override { return QSize(200, 200); }

private:
    QRectF mapToWidget() const; // scaled QRectF of the map image inside this widget

    const Map*     m_map     = nullptr;
    const Tileset* m_tileset = nullptr;
    QImage         m_image;  // 1px per tile, RGB32
    QRectF         m_viewport; // in tile units
};


// Dock widget wrapping MinimapView.
class Minimap : public QDockWidget {
    Q_OBJECT
public:
    explicit Minimap(QWidget* parent = nullptr);

    MinimapView* view() const { return m_view; }

    void setMap(const Map* m)               { m_view->setMap(m); }
    void setTileset(const Tileset* ts)      { m_view->setTileset(ts); }
    void setViewportRect(const QRectF& r)   { m_view->setViewportRect(r); }
    void rebuildImage()                     { m_view->rebuildImage(); }

signals:
    void panRequested(QPointF tilePt);

private:
    MinimapView* m_view;
};
