#include "stamppanel.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

// ---------------------------------------------------------------------------
// StampWidget

StampWidget::StampWidget(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setFocusPolicy(Qt::ClickFocus);
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
    m_selected = int(m_stamps.size()) - 1;
    updateGeometry();
    update();
    emit stampSelected(&m_stamps.back());
}

void StampWidget::clear()
{
    m_stamps.clear();
    m_selected = -1;
    updateGeometry();
    update();
}

void StampWidget::clearSelection()
{
    m_selected = -1;
    update();
}

void StampWidget::setStamps(std::vector<Stamp> stamps)
{
    m_stamps   = std::move(stamps);
    m_selected = m_stamps.empty() ? -1 : 0;
    updateGeometry();
    update();
    if (m_selected >= 0)
        emit stampSelected(&m_stamps[0]);
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

int StampWidget::heightForWidth(int w) const
{
    const int cell  = THUMB + PADDING;
    const int c     = std::max(1, w / cell);
    const int rows  = (int(m_stamps.size()) + c - 1) / c;
    return std::max(cell + 20, rows * (cell + 20));
}

QSize StampWidget::sizeHint() const
{
    const int cell = THUMB + PADDING;
    const int w = std::max(cell, width());
    return QSize(w, heightForWidth(w));
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

    if (m_stamps.empty()) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, "No stamps\ncaptured yet");
        return;
    }

    if (!m_tileset || !m_tileset->isValid()) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter,
                   QString("%1 stamp(s) loaded\n(open a map to preview)").arg(m_stamps.size()));
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
    if (ev->button() == Qt::RightButton) {
        m_selected = -1;
        update();
        emit stampSelected(nullptr);
        return;
    }
    if (ev->button() != Qt::LeftButton) return;
    const int id = stampAt(ev->pos());
    if (id >= 0) {
        m_selected = id;
        update();
        emit stampSelected(&m_stamps[size_t(id)]);
    }
}

void StampWidget::keyPressEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Escape) {
        m_selected = -1;
        update();
        emit stampSelected(nullptr);
        return;
    }
    // Forward navigation keys to the parent scroll area
    if (auto* sa = qobject_cast<QScrollArea*>(parentWidget() ? parentWidget()->parentWidget() : nullptr)) {
        QScrollBar* sb = sa->verticalScrollBar();
        switch (ev->key()) {
        case Qt::Key_Home:   sb->setValue(sb->minimum()); return;
        case Qt::Key_End:    sb->setValue(sb->maximum()); return;
        case Qt::Key_PageUp:   sb->setValue(sb->value() - sb->pageStep()); return;
        case Qt::Key_PageDown: sb->setValue(sb->value() + sb->pageStep()); return;
        default: break;
        }
    }
    QWidget::keyPressEvent(ev);
}

// ---------------------------------------------------------------------------
// StampPanel

StampPanel::StampPanel(QWidget* parent)
    : QDockWidget("Stamps", parent)
    , m_widget(new StampWidget())
    , m_scroll(new QScrollArea())
    , m_captureBtn(new QPushButton("Capture Selection"))
    , m_saveBtn(new QPushButton("Save…"))
    , m_loadBtn(new QPushButton("Load…"))
{
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                | QDockWidget::DockWidgetClosable);

    m_scroll->setWidget(m_widget);
    m_scroll->setWidgetResizable(false);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->viewport()->installEventFilter(this);

    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->addWidget(m_saveBtn);
    btnRow->addWidget(m_loadBtn);

    auto* container = new QWidget();
    auto* layout    = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(m_captureBtn);
    layout->addLayout(btnRow);
    layout->addWidget(m_scroll);
    setWidget(container);

    connect(m_captureBtn, &QPushButton::clicked,
            this, &StampPanel::captureRequested);
    connect(m_widget, &StampWidget::stampSelected,
            this,     &StampPanel::stampSelected);
    connect(m_saveBtn, &QPushButton::clicked, this, [this]() {
        const Stamp* s = m_widget->selectedStamp();
        if (!s) { QMessageBox::information(this, "No stamp selected", "Select a stamp first."); return; }
        const QString defaultName = s->name.isEmpty() ? "stamp" : s->name;
        QString path = QFileDialog::getSaveFileName(
            this, "Save Stamp", defaultName, "Stamp files (*.stamp.json);;All files (*)");
        if (path.isEmpty()) return;
        if (!path.endsWith(".stamp.json", Qt::CaseInsensitive))
            path += ".stamp.json";
        if (!saveSelectedToFile(path))
            QMessageBox::warning(this, "Save failed", "Could not write:\n" + path);
    });
    connect(m_loadBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, "Load Stamps", {}, "Stamp files (*.stamp.json);;All files (*)");
        if (!path.isEmpty() && !loadFromFile(path))
            QMessageBox::warning(this, "Load failed", "Could not read:\n" + path);
    });
}

bool StampPanel::saveSelectedToFile(const QString& path) const
{
    const Stamp* s = m_widget->selectedStamp();
    if (!s) return false;
    QJsonArray tiles;
    for (uint16_t t : s->tiles)
        tiles.append(int(t));
    QJsonObject obj;
    obj["name"]   = s->name;
    obj["width"]  = s->width;
    obj["height"] = s->height;
    obj["tiles"]  = tiles;
    QJsonObject root;
    root["stamps"] = QJsonArray{obj};
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root).toJson());
    return true;
}

bool StampPanel::loadFromFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (doc.isNull() || !doc.isObject()) return false;

    bool any = false;
    for (const QJsonValue& v : doc.object().value("stamps").toArray()) {
        const QJsonObject o = v.toObject();
        Stamp s;
        s.name   = o.value("name").toString();
        s.width  = o.value("width").toInt();
        s.height = o.value("height").toInt();
        for (const QJsonValue& t : o.value("tiles").toArray())
            s.tiles.push_back(uint16_t(t.toInt()));
        if (s.width > 0 && s.height > 0 &&
            int(s.tiles.size()) == s.width * s.height) {
            m_widget->addStamp(std::move(s));
            any = true;
        }
    }
    return any;
}

void StampPanel::loadFromDirectory(const QString& dir)
{
    const QDir d(dir);
    if (!d.exists()) return;
    for (const QString& fn : d.entryList({"*.stamp.json"}, QDir::Files, QDir::Name))
        loadFromFile(d.filePath(fn));
}

void StampPanel::addStamp(Stamp s)
{
    m_widget->addStamp(std::move(s));
}

void StampPanel::setTileset(const Tileset* ts)
{
    m_widget->setTileset(ts);
}

void StampPanel::clearSelection()
{
    m_widget->clearSelection();
}

const Stamp* StampPanel::selectedStamp() const
{
    return m_widget->selectedStamp();
}

void StampPanel::fitWidgetToViewport()
{
    const int vw = m_scroll->viewport()->width();
    const int h  = m_widget->heightForWidth(vw);
    m_widget->setFixedSize(vw, h);
}

bool StampPanel::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_scroll->viewport() && ev->type() == QEvent::Resize)
        fitWidgetToViewport();
    return QDockWidget::eventFilter(obj, ev);
}
