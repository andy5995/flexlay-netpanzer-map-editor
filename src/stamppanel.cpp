#include "stamppanel.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <algorithm>

// ---------------------------------------------------------------------------
// StampWidget

StampWidget::StampWidget(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
}

void StampWidget::setTileset(const Tileset* ts)
{
    m_tileset     = ts;
    m_atlasPixmap = QPixmap();
    m_selected    = -1;
    update();
}

void StampWidget::addStamp(Stamp s)
{
    m_stamps.push_back(std::move(s));
    updateGeometry();
    update();
}

void StampWidget::clear()
{
    m_stamps.clear();
    m_selected = -1;
    updateGeometry();
    update();
}

const Stamp* StampWidget::selectedStamp() const
{
    if (m_selected < 0 || m_selected >= int(m_stamps.size())) return nullptr;
    return &m_stamps[size_t(m_selected)];
}

int StampWidget::cols() const
{
    const int cell = THUMB + PADDING;
    return std::max(1, width() / cell);
}

QSize StampWidget::sizeHint() const
{
    if (m_stamps.empty()) return QSize(THUMB + PADDING, THUMB + PADDING + 20);
    const int c    = std::max(1, cols());
    const int rows = (int(m_stamps.size()) + c - 1) / c;
    return QSize(c * (THUMB + PADDING), rows * (THUMB + PADDING + 20));
}

int StampWidget::stampAt(QPoint pos) const
{
    const int cell = THUMB + PADDING;
    const int labelH = 20;
    const int c   = cols();
    const int col = pos.x() / cell;
    const int row = pos.y() / (cell + labelH);
    if (col < 0 || col >= c) return -1;
    const int id = row * c + col;
    return (id >= 0 && id < int(m_stamps.size())) ? id : -1;
}

void StampWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 30));

    if (!m_tileset || !m_tileset->isValid() || m_stamps.empty()) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, "No stamps\ncaptured yet");
        return;
    }

    if (m_atlasPixmap.isNull())
        m_atlasPixmap = QPixmap::fromImage(m_tileset->atlas(ATLAS_COLS));

    const int cell   = THUMB + PADDING;
    const int labelH = 20;
    const int c      = cols();

    for (int i = 0; i < int(m_stamps.size()); ++i) {
        const Stamp& s = m_stamps[size_t(i)];
        const int col  = i % c;
        const int row  = i / c;
        const int x    = col * cell + PADDING / 2;
        const int y    = row * (cell + labelH) + PADDING / 2;

        // Background
        const bool sel = (i == m_selected);
        p.fillRect(x, y, THUMB, THUMB, QColor(50, 50, 50));

        // Render stamp tiles into the thumbnail area
        if (!m_atlasPixmap.isNull() && s.width > 0 && s.height > 0) {
            const double scaleX = double(THUMB) / (s.width  * 32);
            const double scaleY = double(THUMB) / (s.height * 32);
            const double scale  = std::min(scaleX, scaleY);
            const int thumbW = int(s.width  * 32 * scale);
            const int thumbH = int(s.height * 32 * scale);
            const int offX   = x + (THUMB - thumbW) / 2;
            const int offY   = y + (THUMB - thumbH) / 2;
            const int tileW  = int(32 * scale);
            const int tileH  = int(32 * scale);

            for (int row2 = 0; row2 < s.height; ++row2) {
                for (int col2 = 0; col2 < s.width; ++col2) {
                    const int id  = s.tiles[size_t(row2 * s.width + col2)];
                    const QRect src = m_tileset->atlasRect(id, ATLAS_COLS);
                    p.drawPixmap(offX + col2 * tileW, offY + row2 * tileH, tileW, tileH,
                                 m_atlasPixmap, src.x(), src.y(), src.width(), src.height());
                }
            }
        }

        // Selection border
        if (sel) {
            p.setPen(QPen(QColor(255, 220, 0), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRect(x, y, THUMB, THUMB);
        }

        // Label
        p.setPen(Qt::lightGray);
        QFont f = p.font();
        f.setPixelSize(11);
        p.setFont(f);
        p.drawText(x, y + THUMB + 2, THUMB, labelH - 2,
                   Qt::AlignHCenter | Qt::AlignTop | Qt::TextSingleLine,
                   s.name.isEmpty() ? QString("Stamp %1").arg(i + 1) : s.name);
    }
}

void StampWidget::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) return;
    const int id = stampAt(ev->pos());
    if (id >= 0) {
        m_selected = id;
        update();
        emit stampSelected(&m_stamps[size_t(id)]);
    }
}

// ---------------------------------------------------------------------------
// StampPanel

StampPanel::StampPanel(QWidget* parent)
    : QDockWidget("Stamps", parent)
    , m_widget(new StampWidget())
    , m_scroll(new QScrollArea())
    , m_captureBtn(new QPushButton("Capture Selection"))
{
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                | QDockWidget::DockWidgetClosable);

    m_scroll->setWidget(m_widget);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* container = new QWidget();
    auto* layout    = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(m_captureBtn);
    layout->addWidget(m_scroll);
    setWidget(container);

    connect(m_captureBtn, &QPushButton::clicked,
            this, &StampPanel::captureRequested);
    connect(m_widget, &StampWidget::stampSelected,
            this,     &StampPanel::stampSelected);
}

void StampPanel::addStamp(Stamp s)
{
    m_widget->addStamp(std::move(s));
}

void StampPanel::setTileset(const Tileset* ts)
{
    m_widget->setTileset(ts);
}

const Stamp* StampPanel::selectedStamp() const
{
    return m_widget->selectedStamp();
}
