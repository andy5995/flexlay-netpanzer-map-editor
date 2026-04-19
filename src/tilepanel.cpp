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
    const int available = std::max(width(), RENDER_SIZE);
    return std::max(1, available / RENDER_SIZE);
}

void TilePanelWidget::setSelectedTile(int id)
{
    if (id == m_selectedTile) return;
    m_selectedTile = id;
    update();
}

QSize TilePanelWidget::sizeHint() const
{
    if (!m_tileset || !m_tileset->isValid())
        return QSize(4 * RENDER_SIZE, 200);
    const int c = std::max(1, 4);
    const int rows = (m_tileset->tileCount() + c - 1) / c;
    return QSize(c * RENDER_SIZE, rows * RENDER_SIZE);
}

void TilePanelWidget::setTileset(const Tileset* ts)
{
    m_tileset      = ts;
    m_atlasPixmap  = QPixmap();
    m_selectedTile = 0;
    m_hoveredTile  = -1;
    m_dragging     = false;
    updateGeometry();
    update();
}

QPoint TilePanelWidget::gridPos(QPoint pos) const
{
    return QPoint(pos.x() / RENDER_SIZE, pos.y() / RENDER_SIZE);
}

int TilePanelWidget::tileAt(QPoint pos) const
{
    return tileIdAtGrid(pos.x() / RENDER_SIZE, pos.y() / RENDER_SIZE);
}

int TilePanelWidget::tileIdAtGrid(int col, int row) const
{
    if (!m_tileset || !m_tileset->isValid()) return -1;
    const int c = cols();
    if (col < 0 || col >= c || row < 0) return -1;
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

    if (m_atlasPixmap.isNull())
        m_atlasPixmap = QPixmap::fromImage(m_tileset->atlas(ATLAS_COLS));

    const int c    = cols();
    const int rows = (m_tileset->tileCount() + c - 1) / c;

    const QRect dirty    = ev->rect();
    const int firstRow   = std::max(0, dirty.top()    / RENDER_SIZE);
    const int lastRow    = std::min(rows - 1, dirty.bottom() / RENDER_SIZE);

    const int selC0 = m_dragging ? std::min(m_selStart.x(), m_selEnd.x()) : -1;
    const int selC1 = m_dragging ? std::max(m_selStart.x(), m_selEnd.x()) : -1;
    const int selR0 = m_dragging ? std::min(m_selStart.y(), m_selEnd.y()) : -1;
    const int selR1 = m_dragging ? std::max(m_selStart.y(), m_selEnd.y()) : -1;

    for (int row = firstRow; row <= lastRow; ++row) {
        for (int col = 0; col < c; ++col) {
            const int id = row * c + col;
            if (id >= m_tileset->tileCount()) break;

            const QRect dst(col * RENDER_SIZE, row * RENDER_SIZE,
                            RENDER_SIZE, RENDER_SIZE);
            const QRect src = m_tileset->atlasRect(id, ATLAS_COLS);
            p.drawPixmap(dst, m_atlasPixmap, src);

            const bool inSel = m_dragging &&
                               col >= selC0 && col <= selC1 &&
                               row >= selR0 && row <= selR1;
            if (inSel) {
                p.fillRect(dst, QColor(255, 220, 0, 80));
            }
            if (id == m_selectedTile && !m_dragging) {
                p.setPen(QPen(QColor(255, 220, 0), 2));
                p.setBrush(Qt::NoBrush);
                p.drawRect(dst.adjusted(1, 1, -1, -1));
            }
            if (id == m_hoveredTile && id != m_selectedTile && !m_dragging) {
                p.fillRect(dst, QColor(255, 255, 255, 40));
            }
        }
    }

    if (m_dragging) {
        const QRect selRect(selC0 * RENDER_SIZE, selR0 * RENDER_SIZE,
                            (selC1 - selC0 + 1) * RENDER_SIZE,
                            (selR1 - selR0 + 1) * RENDER_SIZE);
        p.setPen(QPen(QColor(255, 220, 0), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(selRect.adjusted(1, 1, -1, -1));
    }
}

void TilePanelWidget::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) return;
    const QPoint gp = gridPos(ev->pos());
    if (tileIdAtGrid(gp.x(), gp.y()) < 0) return;
    m_dragging = true;
    m_selStart = m_selEnd = gp;
    update();
}

void TilePanelWidget::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_dragging) {
        const QPoint gp = gridPos(ev->pos());
        if (gp != m_selEnd) {
            m_selEnd = gp;
            update();
        }
        return;
    }
    const int id = tileAt(ev->pos());
    if (id != m_hoveredTile) {
        m_hoveredTile = id;
        update();
    }
}

void TilePanelWidget::mouseReleaseEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton || !m_dragging) return;
    m_dragging = false;

    const int c0 = std::min(m_selStart.x(), m_selEnd.x());
    const int c1 = std::max(m_selStart.x(), m_selEnd.x());
    const int r0 = std::min(m_selStart.y(), m_selEnd.y());
    const int r1 = std::max(m_selStart.y(), m_selEnd.y());
    const int w  = c1 - c0 + 1;
    const int h  = r1 - r0 + 1;

    if (w == 1 && h == 1) {
        const int id = tileIdAtGrid(c0, r0);
        if (id >= 0) {
            m_selectedTile = id;
            update();
            emit tileSelected(id);
        }
    } else {
        Stamp s;
        s.width  = w;
        s.height = h;
        s.tiles.resize(size_t(w * h));
        for (int row = 0; row < h; ++row) {
            for (int col = 0; col < w; ++col) {
                const int id = tileIdAtGrid(c0 + col, r0 + row);
                s.tiles[size_t(row * w + col)] = uint16_t(id >= 0 ? id : 0);
            }
        }
        update();
        emit stampCreated(std::move(s));
    }
}

// ---------------------------------------------------------------------------
// TilePanel (QDockWidget)

TilePanel::TilePanel(QWidget* parent)
    : QDockWidget("Tiles", parent)
    , m_widget(new TilePanelWidget())
    , m_scroll(new QScrollArea())
{
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_scroll->setWidget(m_widget);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setWidget(m_scroll);

    connect(m_widget, &TilePanelWidget::tileSelected,
            this,     &TilePanel::tileSelected);
    connect(m_widget, &TilePanelWidget::stampCreated,
            this,     &TilePanel::stampCreated);
}

void TilePanel::setTileset(const Tileset* ts)
{
    m_widget->setTileset(ts);
}

void TilePanel::setSelectedTile(int id)
{
    m_widget->setSelectedTile(id);
    const int c   = m_widget->cols();
    const int col = id % c;
    const int row = id / c;
    const int x   = col * TilePanelWidget::RENDER_SIZE;
    const int y   = row * TilePanelWidget::RENDER_SIZE;
    m_scroll->ensureVisible(x, y, TilePanelWidget::RENDER_SIZE, TilePanelWidget::RENDER_SIZE);
}
