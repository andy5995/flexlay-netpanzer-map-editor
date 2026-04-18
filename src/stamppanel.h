#pragma once
#include <QDockWidget>
#include <QWidget>
#include <QPixmap>
#include <QScrollArea>
#include <QPushButton>
#include <vector>
#include "stamp.h"
#include "tlsloader.h"

class StampWidget : public QWidget {
    Q_OBJECT
public:
    explicit StampWidget(QWidget* parent = nullptr);

    void addStamp(Stamp s);
    void setTileset(const Tileset* ts);
    void clear();

    const Stamp* selectedStamp() const;

signals:
    void stampSelected(const Stamp* stamp);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    QSize sizeHint() const override;

private:
    static constexpr int THUMB   = 96;
    static constexpr int PADDING = 6;
    static constexpr int ATLAS_COLS = 64;
    int cols() const;
    int stampAt(QPoint pos) const;

    const Tileset*     m_tileset = nullptr;
    QPixmap            m_atlasPixmap;
    std::vector<Stamp> m_stamps;
    int                m_selected = -1;
};

class StampPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit StampPanel(QWidget* parent = nullptr);

    void addStamp(Stamp s);
    void setTileset(const Tileset* ts);
    const Stamp* selectedStamp() const;

signals:
    void stampSelected(const Stamp* stamp);
    void captureRequested();

private:
    StampWidget*  m_widget;
    QScrollArea*  m_scroll;
    QPushButton*  m_captureBtn;
};
