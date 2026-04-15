#include "minimap.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <algorithm>

// ---------------------------------------------------------------------------
// MinimapView

MinimapView::MinimapView(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void MinimapView::setMap(const Map* m)
{
    m_map = m;
    rebuildImage();
    update();
}

void MinimapView::setTileset(const Tileset* ts)
{
    m_tileset = ts;
    rebuildImage();
    update();
}

void MinimapView::setViewportRect(const QRectF& tileRect)
{
    m_viewport = tileRect;
    update();
}

// ---------------------------------------------------------------------------
// Build a 1px-per-tile QImage of the map.

void MinimapView::rebuildImage()
{
    if (!m_map || !m_map->isValid()) {
        m_image = QImage();
        return;
    }

    const int W = m_map->width;
    const int H = m_map->height;
    m_image = QImage(W, H, QImage::Format_RGB32);

    const QVector<QRgb>* palette = (m_tileset && m_tileset->isValid())
                                   ? &m_tileset->palette() : nullptr;

    for (int y = 0; y < H; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(m_image.scanLine(y));
        for (int x = 0; x < W; ++x) {
            const int id = m_map->tiles[size_t(y * W + x)];
            QRgb col;
            if (palette && id < m_tileset->tileCount()) {
                // Use avg_color (palette index stored in the tile header)
                const int avgIdx = uint8_t(m_tileset->header(id).avg_color);
                col = (*palette)[avgIdx];
            } else {
                // Fallback: HSV from tile id
                col = QColor::fromHsv((id * 37) % 360, 180, 200).rgb();
            }
            line[x] = col;
        }
    }
    update();
}

// ---------------------------------------------------------------------------
// Map image → widget coordinate mapping (letterboxed, centred)

QRectF MinimapView::mapToWidget() const
{
    if (m_image.isNull()) return {};
    const double sw = double(width())      / m_image.width();
    const double sh = double(height() - 2) / m_image.height();
    const double s  = std::min(sw, sh);
    const double dw = m_image.width()  * s;
    const double dh = m_image.height() * s;
    return QRectF((width() - dw) / 2.0, (height() - dh) / 2.0, dw, dh);
}

// ---------------------------------------------------------------------------
// Paint

void MinimapView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 30));

    if (m_image.isNull()) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, "No map");
        return;
    }

    const QRectF dst = mapToWidget();
    p.drawImage(dst, m_image, QRectF(m_image.rect()));

    // Viewport overlay
    if (!m_viewport.isEmpty() && m_map && m_map->isValid()) {
        const double scaleX = dst.width()  / m_map->width;
        const double scaleY = dst.height() / m_map->height;
        const QRectF vr(
            dst.left() + m_viewport.left() * scaleX,
            dst.top()  + m_viewport.top()  * scaleY,
            m_viewport.width()  * scaleX,
            m_viewport.height() * scaleY);

        p.setPen(QPen(QColor(255, 220, 0, 200), 1.5));
        p.setBrush(QColor(255, 220, 0, 30));
        p.drawRect(vr.intersected(dst));
    }

    // Objects
    if (m_map) {
        const QRectF dst2 = mapToWidget();
        const double sx = dst2.width()  / m_map->width;
        const double sy = dst2.height() / m_map->height;
        for (const auto& obj : m_map->objects) {
            const double cx = dst2.left() + (obj.x + 0.5) * sx;
            const double cy = dst2.top()  + (obj.y + 0.5) * sy;
            const double r  = std::max(2.0, std::min(sx, sy) * 0.6);
            QColor col = (obj.type == "outpost") ? QColor(255, 80, 80)
                                                 : QColor(80, 120, 255);
            p.setBrush(col);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(cx, cy), r, r);
        }
    }
}

// ---------------------------------------------------------------------------
// Mouse: click to pan the main view

void MinimapView::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton || m_image.isNull() || !m_map) return;
    const QRectF dst = mapToWidget();
    const double tx = (ev->pos().x() - dst.left()) / dst.width()  * m_map->width;
    const double ty = (ev->pos().y() - dst.top())  / dst.height() * m_map->height;
    emit panRequested(QPointF(tx, ty));
}

void MinimapView::mouseMoveEvent(QMouseEvent* ev)
{
    if ((ev->buttons() & Qt::LeftButton) && !m_image.isNull() && m_map) {
        const QRectF dst = mapToWidget();
        const double tx = (ev->pos().x() - dst.left()) / dst.width()  * m_map->width;
        const double ty = (ev->pos().y() - dst.top())  / dst.height() * m_map->height;
        emit panRequested(QPointF(tx, ty));
    }
}

// ---------------------------------------------------------------------------
// Minimap dock

Minimap::Minimap(QWidget* parent)
    : QDockWidget("Minimap", parent)
    , m_view(new MinimapView())
{
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setWidget(m_view);
    connect(m_view, &MinimapView::panRequested,
            this,   &Minimap::panRequested);
}
