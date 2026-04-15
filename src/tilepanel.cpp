#include "tilepanel.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>

// ---------------------------------------------------------------------------
// TilePanelWidget

TilePanelWidget::TilePanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
}

int TilePanelWidget::cols() const
{
    // Use as many columns as fit into our current width, minimum 1.
    const int available = std::max(width(), RENDER_SIZE);
    return std::max(1, available / RENDER_SIZE);
}

QSize TilePanelWidget::sizeHint() const
{
    if (!m_tileset || !m_tileset->isValid())
        return QSize(4 * RENDER_SIZE, 200);
    const int c = std::max(1, 4); // use 4 as default col count for hint
    const int rows = (m_tileset->tileCount() + c - 1) / c;
    return QSize(c * RENDER_SIZE, rows * RENDER_SIZE);
}

void TilePanelWidget::setTileset(const Tileset* ts)
{
    m_tileset   = ts;
    m_atlasPixmap = QPixmap();
    m_selectedTile = 0;
    m_hoveredTile  = -1;
    updateGeometry();
    update();
}

int TilePanelWidget::tileAt(QPoint pos) const
{
    if (!m_tileset || !m_tileset->isValid()) return -1;
    const int c = cols();
    const int col = pos.x() / RENDER_SIZE;
    const int row = pos.y() / RENDER_SIZE;
    if (col < 0 || col >= c) return -1;
    const int id = row * c + col;
    return (id >= 0 && id < m_tileset->tileCount()) ? id : -1;
}

void TilePanelWidget::paintEvent(QPaintEvent* ev)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 30));

    if (!m_tileset || !m_tileset->isValid()) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, "No tileset");
        return;
    }

    // Build atlas pixmap on first use
    if (m_atlasPixmap.isNull())
        m_atlasPixmap = QPixmap::fromImage(m_tileset->atlas(ATLAS_COLS));

    const int c    = cols();
    const int rows = (m_tileset->tileCount() + c - 1) / c;

    // Only repaint tiles that intersect the dirty region
    const QRect dirty = ev->rect();
    const int firstRow = std::max(0, dirty.top()    / RENDER_SIZE);
    const int lastRow  = std::min(rows - 1, dirty.bottom() / RENDER_SIZE);

    for (int row = firstRow; row <= lastRow; ++row) {
        for (int col = 0; col < c; ++col) {
            const int id = row * c + col;
            if (id >= m_tileset->tileCount()) break;

            const QRect dst(col * RENDER_SIZE, row * RENDER_SIZE,
                            RENDER_SIZE, RENDER_SIZE);
            const QRect src = m_tileset->atlasRect(id, ATLAS_COLS);
            p.drawPixmap(dst, m_atlasPixmap, src);

            // Highlight selected tile
            if (id == m_selectedTile) {
                p.setPen(QPen(QColor(255, 220, 0), 2));
                p.setBrush(Qt::NoBrush);
                p.drawRect(dst.adjusted(1, 1, -1, -1));
            }
            // Highlight hovered tile (dimly)
            if (id == m_hoveredTile && id != m_selectedTile) {
                p.fillRect(dst, QColor(255, 255, 255, 40));
            }
        }
    }
}

void TilePanelWidget::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton) {
        const int id = tileAt(ev->pos());
        if (id >= 0) {
            m_selectedTile = id;
            update();
            emit tileSelected(id);
        }
    }
}

void TilePanelWidget::mouseMoveEvent(QMouseEvent* ev)
{
    const int id = tileAt(ev->pos());
    if (id != m_hoveredTile) {
        m_hoveredTile = id;
        update();
    }
}

// ---------------------------------------------------------------------------
// TilePanel (QDockWidget)

TilePanel::TilePanel(QWidget* parent)
    : QDockWidget("Tiles", parent)
    , m_widget(new TilePanelWidget())
{
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    auto* scroll = new QScrollArea();
    scroll->setWidget(m_widget);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setWidget(scroll);

    connect(m_widget, &TilePanelWidget::tileSelected,
            this,     &TilePanel::tileSelected);
}

void TilePanel::setTileset(const Tileset* ts)
{
    m_widget->setTileset(ts);
}
