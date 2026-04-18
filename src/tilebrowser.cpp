#include "tilebrowser.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <algorithm>

// ---------------------------------------------------------------------------
// TileBrowserWidget

TileBrowserWidget::TileBrowserWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
}

void TileBrowserWidget::setTileset(const Tileset* ts)
{
    m_tileset     = ts;
    m_atlasPixmap = QPixmap();
    m_selectedTile = 0;
    m_hoveredTile  = -1;
    updateGeometry();
    update();
}

void TileBrowserWidget::setTileSize(int px)
{
    m_tileSize = px;
    updateGeometry();
    update();
}

int TileBrowserWidget::cols() const
{
    const int available = std::max(width(), m_tileSize);
    return std::max(1, available / m_tileSize);
}

QSize TileBrowserWidget::sizeHint() const
{
    if (!m_tileset || !m_tileset->isValid())
        return QSize(4 * m_tileSize, 200);
    const int c    = std::max(1, 4);
    const int rows = (m_tileset->tileCount() + c - 1) / c;
    return QSize(c * m_tileSize, rows * m_tileSize);
}

QPoint TileBrowserWidget::gridPos(QPoint widgetPos) const
{
    return QPoint(widgetPos.x() / m_tileSize, widgetPos.y() / m_tileSize);
}

int TileBrowserWidget::tileIdAt(QPoint grid) const
{
    if (!m_tileset || !m_tileset->isValid()) return -1;
    const int c = cols();
    if (grid.x() < 0 || grid.x() >= c || grid.y() < 0) return -1;
    const int id = grid.y() * c + grid.x();
    return (id < m_tileset->tileCount()) ? id : -1;
}

void TileBrowserWidget::paintEvent(QPaintEvent* ev)
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

    const QRect dirty = ev->rect();
    const int firstRow = std::max(0, dirty.top()    / m_tileSize);
    const int lastRow  = std::min(rows - 1, dirty.bottom() / m_tileSize);

    // Pre-compute the drag-selection rect in grid coords (normalised)
    const int selC0 = m_dragging ? std::min(m_selStart.x(), m_selEnd.x()) : -1;
    const int selC1 = m_dragging ? std::max(m_selStart.x(), m_selEnd.x()) : -1;
    const int selR0 = m_dragging ? std::min(m_selStart.y(), m_selEnd.y()) : -1;
    const int selR1 = m_dragging ? std::max(m_selStart.y(), m_selEnd.y()) : -1;

    for (int row = firstRow; row <= lastRow; ++row) {
        for (int col = 0; col < c; ++col) {
            const int id = row * c + col;
            if (id >= m_tileset->tileCount()) break;

            const QRect dst(col * m_tileSize, row * m_tileSize, m_tileSize, m_tileSize);
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

    // Selection border
    if (m_dragging) {
        const QRect selRect(selC0 * m_tileSize, selR0 * m_tileSize,
                            (selC1 - selC0 + 1) * m_tileSize,
                            (selR1 - selR0 + 1) * m_tileSize);
        p.setPen(QPen(QColor(255, 220, 0), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(selRect.adjusted(1, 1, -1, -1));
    }
}

void TileBrowserWidget::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) return;
    const QPoint gp = gridPos(ev->pos());
    if (tileIdAt(gp) < 0) return;
    m_dragging = true;
    m_selStart = m_selEnd = gp;
    update();
}

void TileBrowserWidget::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_dragging) {
        const QPoint gp = gridPos(ev->pos());
        if (gp != m_selEnd) {
            m_selEnd = gp;
            update();
        }
        return;
    }
    const int id = tileIdAt(gridPos(ev->pos()));
    if (id != m_hoveredTile) {
        m_hoveredTile = id;
        update();
    }
}

void TileBrowserWidget::mouseReleaseEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton || !m_dragging) return;
    m_dragging = false;

    const int colC0 = std::min(m_selStart.x(), m_selEnd.x());
    const int colC1 = std::max(m_selStart.x(), m_selEnd.x());
    const int rowR0 = std::min(m_selStart.y(), m_selEnd.y());
    const int rowR1 = std::max(m_selStart.y(), m_selEnd.y());
    const int w     = colC1 - colC0 + 1;
    const int h     = rowR1 - rowR0 + 1;

    if (w == 1 && h == 1) {
        // Single tile — treat as a normal click
        const int id = tileIdAt(m_selStart);
        if (id >= 0) {
            m_selectedTile = id;
            emit tileSelected(id);
        }
    } else {
        // Multi-tile — build a stamp
        Stamp s;
        s.width  = w;
        s.height = h;
        s.tiles.resize(size_t(w * h));
        for (int row = 0; row < h; ++row) {
            for (int col = 0; col < w; ++col) {
                const int id = tileIdAt(QPoint(colC0 + col, rowR0 + row));
                s.tiles[size_t(row * w + col)] =
                    uint16_t(id >= 0 ? id : 0);
            }
        }
        emit stampCreated(std::move(s));
    }
    update();
}

void TileBrowserWidget::leaveEvent(QEvent*)
{
    m_hoveredTile = -1;
    update();
}

// ---------------------------------------------------------------------------
// TileBrowser

TileBrowser::TileBrowser(QWidget* parent)
    : QDockWidget("Tile Browser", parent)
    , m_widget(new TileBrowserWidget())
    , m_scroll(new QScrollArea())
{
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                | QDockWidget::DockWidgetClosable);

    auto* sizeCombo = new QComboBox();
    sizeCombo->addItem("Small (32px)",  32);
    sizeCombo->addItem("Medium (64px)", 64);
    sizeCombo->addItem("Large (96px)",  96);
    sizeCombo->setCurrentIndex(1);

    m_scroll->setWidget(m_widget);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* container = new QWidget();
    auto* layout    = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(sizeCombo);
    layout->addWidget(m_scroll);
    setWidget(container);

    connect(sizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, sizeCombo](int) {
        m_widget->setTileSize(sizeCombo->currentData().toInt());
    });

    connect(m_widget, &TileBrowserWidget::tileSelected,
            this,     &TileBrowser::tileSelected);
    connect(m_widget, &TileBrowserWidget::stampCreated,
            this,     &TileBrowser::stampCreated);
}

void TileBrowser::setTileset(const Tileset* ts)
{
    m_widget->setTileset(ts);
}
