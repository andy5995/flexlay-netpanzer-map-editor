#pragma once
#include <QMainWindow>
#include <QString>
#include "maploader.h"
#include "tlsloader.h"
#include "mapview.h"   // for Tool enum

class MapView;
class TilePanel;
class TileBrowser;
class StampPanel;
class Minimap;
class QLabel;
class QAction;
class QActionGroup;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onOpen();
    void onSave();
    void onSaveAs();
    void onNewMap();
    void onLoadTileset();
    void onUndo();
    void onRedo();
    void onSetTool(Tool t);
    void onToggleGrid(bool checked);
    void onFitToWindow();
    void onZoomIn();
    void onZoomOut();
    void onTileHovered(int tx, int ty, int tileId);
    void onMapModified();
    void onObjectSelectionChanged(int idx);
    void onObjectActivated(int idx);
    void onViewportChanged(const QRectF& tileRect);
    void onMinimapPan(QPointF tilePt);

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    void setupMenus();
    void setupToolbar();
    void setupStatusBar();
    void setCurrentFile(const QString& path);
    void updateTitle();
    void applyTileset();
    QString findTileset(const QString& mapPath, const QString& tileSetName) const;
    // Returns true if it is safe to discard the current map (no unsaved changes,
    // or the user chose to save/discard). Returns false if the user cancelled.
    bool maybeSave();

    MapView*      m_view        = nullptr;
    TilePanel*    m_tilePanel   = nullptr;
    TileBrowser*  m_tileBrowser = nullptr;
    StampPanel*   m_stampPanel  = nullptr;
    Minimap*      m_minimap     = nullptr;
    QLabel*       m_statusTile = nullptr;
    QLabel*       m_statusZoom = nullptr;
    QLabel*       m_statusObj  = nullptr;

    QAction*      m_undoAct   = nullptr;
    QAction*      m_redoAct   = nullptr;
    QAction*      m_saveAct   = nullptr;
    QActionGroup* m_toolGroup = nullptr;

    QString m_currentFile;
    bool    m_modified = false;

    Tileset m_tileset;
};
