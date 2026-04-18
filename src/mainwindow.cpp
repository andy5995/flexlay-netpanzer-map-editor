#include "mainwindow.h"
#include "mapview.h"
#include "tilepanel.h"
#include "tilebrowser.h"
#include "stamppanel.h"
#include "minimap.h"
#include "maploader.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QLineEdit>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QActionGroup>
#include <QKeySequence>
#include <QCloseEvent>
#include <QTime>
#include <cmath>

// ---------------------------------------------------------------------------
// Constructor

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("netPanzer Map Editor");
    resize(1280, 800);

    m_view = new MapView(this);
    setCentralWidget(m_view);

    m_tilePanel = new TilePanel(this);
    addDockWidget(Qt::RightDockWidgetArea, m_tilePanel);

    m_tileBrowser = new TileBrowser(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_tileBrowser);
    m_tileBrowser->hide();  // hidden by default, opened from View menu

    m_stampPanel = new StampPanel(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_stampPanel);
    m_stampPanel->hide();   // hidden by default

    m_minimap = new Minimap(this);
    addDockWidget(Qt::RightDockWidgetArea, m_minimap);

    setupMenus();
    setupToolbar();
    setupStatusBar();

    connect(m_view, &MapView::tileHovered,            this, &MainWindow::onTileHovered);
    connect(m_view, &MapView::mapModified,            this, &MainWindow::onMapModified);
    connect(m_view, &MapView::objectSelectionChanged, this, &MainWindow::onObjectSelectionChanged);
    connect(m_view, &MapView::objectActivated,        this, &MainWindow::onObjectActivated);
    connect(m_view, &MapView::viewportChanged,        this, &MainWindow::onViewportChanged);

    connect(m_tilePanel,   &TilePanel::tileSelected,   m_view, &MapView::setSelectedTile);
    connect(m_tileBrowser, &TileBrowser::tileSelected, this, [this](int id) {
        m_view->setSelectedTile(id);
        m_tilePanel->setSelectedTile(id);
        m_view->setTool(Tool::TilePaint);
    });
    connect(m_tileBrowser, &TileBrowser::stampCreated, this, [this](Stamp s) {
        m_stampPanel->addStamp(std::move(s));
        m_view->setCurrentStamp(m_stampPanel->selectedStamp());
        m_view->setTool(Tool::StampPaint);
        m_stampPanel->show();
    });
    connect(m_view, &MapView::tilePicked, this, [this](int id) {
        m_view->setSelectedTile(id);
        m_tilePanel->setSelectedTile(id);
    });
    connect(m_stampPanel, &StampPanel::captureRequested, this, [this]() {
        if (!m_view->hasSelection()) {
            QMessageBox::information(this, "No selection",
                "Use the Rect Select tool (R) to drag a region on the map first.");
            return;
        }
        Stamp s = m_view->captureSelection();
        s.name = QString("Stamp %1").arg(QTime::currentTime().toString("hh:mm:ss"));
        m_stampPanel->addStamp(std::move(s));
    });
    connect(m_stampPanel, &StampPanel::stampSelected, this, [this](const Stamp* stamp) {
        m_view->setCurrentStamp(stamp);
        m_view->setTool(Tool::StampPaint);
    });
    connect(m_minimap,   &Minimap::panRequested,   this,   &MainWindow::onMinimapPan);
}

// ---------------------------------------------------------------------------
// Menus

void MainWindow::setupMenus()
{
    // File
    QMenu* file = menuBar()->addMenu("&File");

    QAction* newAct = file->addAction("&New Map…");
    newAct->setShortcut(QKeySequence::New);
    connect(newAct, &QAction::triggered, this, &MainWindow::onNewMap);

    QAction* openAct = file->addAction("&Open…");
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpen);

    file->addSeparator();

    m_saveAct = file->addAction("&Save");
    m_saveAct->setShortcut(QKeySequence::Save);
    m_saveAct->setEnabled(false);
    connect(m_saveAct, &QAction::triggered, this, &MainWindow::onSave);

    QAction* saveAsAct = file->addAction("Save &As…");
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::onSaveAs);

    file->addSeparator();

    QAction* loadTsAct = file->addAction("Load &Tileset…");
    connect(loadTsAct, &QAction::triggered, this, &MainWindow::onLoadTileset);

    file->addSeparator();

    QAction* quitAct = file->addAction("&Quit");
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    // Edit
    QMenu* edit = menuBar()->addMenu("&Edit");

    m_undoAct = edit->addAction("&Undo");
    m_undoAct->setShortcut(QKeySequence::Undo);
    m_undoAct->setEnabled(false);
    connect(m_undoAct, &QAction::triggered, this, &MainWindow::onUndo);

    m_redoAct = edit->addAction("&Redo");
    m_redoAct->setShortcut(QKeySequence::Redo);
    m_redoAct->setEnabled(false);
    connect(m_redoAct, &QAction::triggered, this, &MainWindow::onRedo);

    edit->addSeparator();

    QAction* delObjAct = edit->addAction("&Delete Object");
    delObjAct->setShortcut(Qt::Key_Delete);
    connect(delObjAct, &QAction::triggered,
            m_view, &MapView::deleteSelectedObject);

    // Tools
    QMenu* tools = menuBar()->addMenu("&Tools");
    m_toolGroup = new QActionGroup(this);
    m_toolGroup->setExclusive(true);

    struct ToolDef { const char* label; const char* iconText; Tool tool; QKeySequence key; };
    const ToolDef defs[] = {
        { "Tile &Paint",       "Paint",    Tool::TilePaint,       Qt::Key_T },
        { "Tile P&ick",        "Pick",     Tool::TilePick,        Qt::Key_I },
        { "&Rect Select",      "Rect Sel", Tool::RectSelect,      Qt::Key_R },
        { "&Stamp Paint",      "Stamp",    Tool::StampPaint,      Qt::Key_M },
        { "Place &Outpost",    "Outpost",  Tool::PlaceOutpost,    Qt::Key_O },
        { "Place &Spawnpoint", "Spawn",    Tool::PlaceSpawnpoint, Qt::Key_S },
        { "Se&lect Object",    "Select",   Tool::SelectObject,    Qt::Key_V },
    };
    for (const auto& d : defs) {
        QAction* a = tools->addAction(d.label);
        a->setCheckable(true);
        a->setShortcut(d.key);
        a->setData(int(d.tool));
        a->setIconText(d.iconText);
        m_toolGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, t = d.tool]() { onSetTool(t); });
    }
    m_toolGroup->actions().first()->setChecked(true);

    // View
    QMenu* view = menuBar()->addMenu("&View");

    QAction* gridToggle = view->addAction("Show &Grid");
    gridToggle->setCheckable(true);
    gridToggle->setChecked(true);
    gridToggle->setShortcut(Qt::Key_G);
    connect(gridToggle, &QAction::toggled, this, &MainWindow::onToggleGrid);

    view->addSeparator();

    QAction* fitAct = view->addAction("&Fit to Window");
    fitAct->setShortcut(Qt::Key_F);
    connect(fitAct, &QAction::triggered, this, &MainWindow::onFitToWindow);

    QAction* zoomInAct = view->addAction("Zoom &In");
    zoomInAct->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAct, &QAction::triggered, this, &MainWindow::onZoomIn);

    QAction* zoomOutAct = view->addAction("Zoom &Out");
    zoomOutAct->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAct, &QAction::triggered, this, &MainWindow::onZoomOut);

    view->addSeparator();
    auto* tilePanelAct = m_tilePanel->toggleViewAction();
    tilePanelAct->setText("&Tile Panel");
    view->addAction(tilePanelAct);
    auto* tileBrowserAct = m_tileBrowser->toggleViewAction();
    tileBrowserAct->setText("Tile &Browser");
    view->addAction(tileBrowserAct);
    auto* stampPanelAct = m_stampPanel->toggleViewAction();
    stampPanelAct->setText("&Stamps");
    view->addAction(stampPanelAct);
    auto* minimapAct = m_minimap->toggleViewAction();
    minimapAct->setText("&Minimap");
    view->addAction(minimapAct);
}

// ---------------------------------------------------------------------------
// Toolbar

void MainWindow::setupToolbar()
{
    QToolBar* tb = addToolBar("Main");
    tb->setMovable(false);

    // Zoom
    tb->addAction("Fit",      this, &MainWindow::onFitToWindow);
    tb->addAction("Zoom In",  this, &MainWindow::onZoomIn);
    tb->addAction("Zoom Out", this, &MainWindow::onZoomOut);
    tb->addSeparator();

    // Tools — reuse the same QAction objects from the menu so they share
    // checked state and the exclusive group handles mutual exclusivity.
    for (QAction* a : m_toolGroup->actions())
        tb->addAction(a);
}

// ---------------------------------------------------------------------------
// Status bar

void MainWindow::setupStatusBar()
{
    m_statusObj  = new QLabel();
    m_statusTile = new QLabel("No map");
    m_statusZoom = new QLabel("100%");
    statusBar()->addWidget(m_statusObj, 1);
    statusBar()->addPermanentWidget(m_statusTile);
    statusBar()->addPermanentWidget(m_statusZoom);
}

// ---------------------------------------------------------------------------
// Title / dirty flag

void MainWindow::updateTitle()
{
    QString title = "netPanzer Map Editor";
    if (!m_currentFile.isEmpty())
        title += " — " + QFileInfo(m_currentFile).fileName();
    if (m_modified) title += " *";
    setWindowTitle(title);
}

void MainWindow::setCurrentFile(const QString& path)
{
    m_currentFile = path;
    m_modified    = false;
    m_saveAct->setEnabled(!path.isEmpty());
    updateTitle();
}

// ---------------------------------------------------------------------------
// Tileset search + apply

QString MainWindow::findTileset(const QString& mapPath,
                                const QString& tileSetName) const
{
    if (tileSetName.isEmpty()) return {};
    const QString mapDir = QFileInfo(mapPath).absoluteDir().absolutePath();

    // 1. Same directory as map
    {
        const QString c = mapDir + "/" + tileSetName;
        if (QFileInfo::exists(c)) return c;
    }
    // 2. ../wads/<base>/*/<name>
    {
        const QString base = QFileInfo(tileSetName).baseName();
        QDirIterator it(mapDir + "/../wads/" + base, {tileSetName},
                        QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext()) return it.next();
    }
    // 3. Walk up looking for a wads/ dir
    {
        QDir dir(mapDir);
        for (int i = 0; i < 5; ++i) {
            if (!dir.cdUp()) break;
            const QString base = QFileInfo(tileSetName).baseName();
            QDirIterator it(dir.filePath("wads/" + base), {tileSetName},
                            QDir::Files, QDirIterator::Subdirectories);
            if (it.hasNext()) return it.next();
        }
    }
    return {};
}

void MainWindow::applyTileset()
{
    if (m_tileset.isValid()) {
        m_view->setTileset(&m_tileset);
        m_tilePanel->setTileset(&m_tileset);
        m_tileBrowser->setTileset(&m_tileset);
        m_stampPanel->setTileset(&m_tileset);
        m_minimap->setTileset(&m_tileset);
        m_minimap->rebuildImage();
    } else {
        m_view->setTileset(nullptr);
        m_tilePanel->setTileset(nullptr);
        m_tileBrowser->setTileset(nullptr);
        m_stampPanel->setTileset(nullptr);
        m_minimap->setTileset(nullptr);
    }
}

// ---------------------------------------------------------------------------
// File actions

void MainWindow::onOpen()
{
    if (!maybeSave()) return;

    const QString fn = QFileDialog::getOpenFileName(
        this, "Open map", m_currentFile,
        "netPanzer maps (*.npm);;Text maps (*.txt);;All files (*)");
    if (fn.isEmpty()) return;

    Map m = MapLoader::load(fn);
    if (!m.isValid()) {
        QMessageBox::warning(this, "Open failed", "Could not load:\n" + fn);
        return;
    }

    m_view->setMap(m);
    m_minimap->setMap(&m_view->map());
    setCurrentFile(fn);
    m_undoAct->setEnabled(false);
    m_redoAct->setEnabled(false);

    // Auto-detect tileset
    const QString tsPath = findTileset(fn, m.tileSetName);
    if (!tsPath.isEmpty()) {
        if (m_tileset.load(tsPath)) {
            applyTileset();
            statusBar()->showMessage("Tileset: " + QFileInfo(tsPath).fileName(), 4000);
        } else {
            QMessageBox::warning(this, "Tileset error",
                                 "Found tileset but could not load it:\n" + tsPath +
                                 "\n\nUse File → Load Tileset to locate it manually.");
        }
    } else if (!m.tileSetName.isEmpty()) {
        statusBar()->showMessage(
            "Tileset not found: " + m.tileSetName +
            " — use File → Load Tileset to locate it.", 8000);
    }

    m_view->fitToWindow();
}

// Build a copy of the current map with the thumbnail populated from the
// tileset's avg_color values.  netPanzer's map browser requires a non-zero
// thumbnail (width × height palette-indexed bytes) to list the map.
// For maps loaded from an existing .npm the thumbnail is already present;
// this only fills it in for new maps or maps with a missing thumbnail.
static Map mapWithThumbnail(const Map& map, const Tileset* ts)
{
    Map m = map;
    if (m.thumbW == 0 || m.thumbH == 0 || int(m.thumbnail.size()) < m.width * m.height) {
        m.thumbW = uint16_t(m.width);
        m.thumbH = uint16_t(m.height);
        const int n = m.width * m.height;
        m.thumbnail.resize(n);
        for (int i = 0; i < n; ++i) {
            const uint16_t tid = m.tiles[size_t(i)];
            m.thumbnail[i] = (ts && int(tid) < ts->tileCount())
                              ? char(uint8_t(ts->header(int(tid)).avg_color))
                              : char(0);
        }
    }
    return m;
}

// Returns true if the map passes basic playability checks, or the user
// accepts the warnings and wants to save anyway.
static bool warnIfUnplayable(QWidget* parent, const Map& m)
{
    int spawns   = 0;
    int outposts = 0;
    for (const auto& obj : m.objects) {
        if (obj.type == "spawnpoint") ++spawns;
        if (obj.type == "outpost")    ++outposts;
    }

    QStringList issues;
    if (spawns == 0)
        issues << "No spawn points — the game will crash on start.";
    if (outposts == 0)
        issues << "No outposts — there will be nothing to capture.";

    if (issues.isEmpty()) return true;

    const auto btn = QMessageBox::warning(
        parent, "Map may be unplayable",
        issues.join('\n') + "\n\nSave anyway?",
        QMessageBox::Save | QMessageBox::Cancel,
        QMessageBox::Cancel);
    return btn == QMessageBox::Save;
}

void MainWindow::onSave()
{
    if (m_currentFile.isEmpty()) { onSaveAs(); return; }
    if (!warnIfUnplayable(this, m_view->map())) return;

    const Map m = mapWithThumbnail(m_view->map(), m_view->tileset());

    if (m_currentFile.endsWith(".npm", Qt::CaseInsensitive)) {
        const QString newPath = m_currentFile + ".new";
        if (!MapLoader::saveNpmVerified(m_currentFile, m, false)) {
            QMessageBox::warning(this, "Save failed",
                                 "Failed to write or verify:\n" + newPath);
            return;
        }
        const auto btn = QMessageBox::question(
            this, "Replace original?",
            "Verified map written to:\n" + newPath +
            "\n\nReplace original? (backup created as .bak)",
            QMessageBox::Yes | QMessageBox::No);
        if (btn == QMessageBox::Yes) {
            const QString backup = m_currentFile + ".bak";
            QFile::remove(backup);
            const bool renamed = QFile::rename(m_currentFile, backup)
                              && QFile::rename(newPath, m_currentFile);
            if (!renamed)
                QMessageBox::warning(this, "Replace failed",
                                     "Could not replace original.\nSaved copy is at:\n" + newPath);
            else { m_modified = false; updateTitle(); }
        }
    } else {
        if (!MapLoader::saveText(m_currentFile, m)) {
            QMessageBox::warning(this, "Save failed", "Failed to write:\n" + m_currentFile);
            return;
        }
        m_modified = false;
        updateTitle();
    }
}

void MainWindow::onSaveAs()
{
    if (!warnIfUnplayable(this, m_view->map())) return;

    const QString fn = QFileDialog::getSaveFileName(
        this, "Save map as", m_currentFile,
        "netPanzer maps (*.npm);;Text maps (*.txt);;All files (*)");
    if (fn.isEmpty()) return;

    const Map m = mapWithThumbnail(m_view->map(), m_view->tileset());
    const bool ok = fn.endsWith(".npm", Qt::CaseInsensitive)
                    ? MapLoader::saveNpm(fn, m)
                    : MapLoader::saveText(fn, m);
    if (!ok)
        QMessageBox::warning(this, "Save failed", "Failed to write:\n" + fn);
    else {
        setCurrentFile(fn);
        statusBar()->showMessage("Saved: " + fn, 3000);
    }
}

void MainWindow::onNewMap()
{
    if (!maybeSave()) return;

    QDialog dlg(this);
    dlg.setWindowTitle("New Map");
    auto* layout = new QFormLayout(&dlg);

    auto* nameEdit = new QLineEdit("Unnamed");
    auto* wSpin    = new QSpinBox(); wSpin->setRange(16, 4096); wSpin->setValue(128);
    auto* hSpin    = new QSpinBox(); hSpin->setRange(16, 4096); hSpin->setValue(128);
    auto* tsEdit   = new QLineEdit("summer12mb.tls");

    layout->addRow("Name:",    nameEdit);
    layout->addRow("Width:",   wSpin);
    layout->addRow("Height:",  hSpin);
    layout->addRow("Tileset:", tsEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    Map m;
    m.width       = wSpin->value();
    m.height      = hSpin->value();
    m.name        = nameEdit->text();
    m.tileSetName = tsEdit->text();
    m.tiles.assign(size_t(m.width * m.height), 0);

    m_view->setMap(m);
    m_minimap->setMap(&m_view->map());
    m_minimap->rebuildImage();
    setCurrentFile({});
    m_undoAct->setEnabled(false);
    m_redoAct->setEnabled(false);
}

void MainWindow::onLoadTileset()
{
    const QString fn = QFileDialog::getOpenFileName(
        this, "Load tileset", {},
        "netPanzer tilesets (*.tls);;All files (*)");
    if (fn.isEmpty()) return;

    if (!m_tileset.load(fn)) {
        QMessageBox::warning(this, "Load failed", "Could not load:\n" + fn);
        return;
    }
    applyTileset();
    statusBar()->showMessage("Tileset: " + QFileInfo(fn).fileName(), 4000);
}

// ---------------------------------------------------------------------------
// Unsaved-changes guard

bool MainWindow::maybeSave()
{
    if (!m_modified) return true;
    const auto btn = QMessageBox::question(
        this, "Unsaved Changes",
        "The map has unsaved changes.\nDo you want to save before continuing?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (btn == QMessageBox::Cancel)  return false;
    if (btn == QMessageBox::Discard) return true;
    onSave();
    return !m_modified; // false if the save itself was cancelled or failed
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    if (maybeSave())
        e->accept();
    else
        e->ignore();
}

// ---------------------------------------------------------------------------
// Edit actions

void MainWindow::onUndo()
{
    m_view->undo();
    m_undoAct->setEnabled(m_view->canUndo());
    m_redoAct->setEnabled(m_view->canRedo());
    m_minimap->rebuildImage();
}

void MainWindow::onRedo()
{
    m_view->redo();
    m_undoAct->setEnabled(m_view->canUndo());
    m_redoAct->setEnabled(m_view->canRedo());
    m_minimap->rebuildImage();
}

// ---------------------------------------------------------------------------
// Tool selection

void MainWindow::onSetTool(Tool t)
{
    m_view->setTool(t);
    // Sync tile panel: only useful when painting
    m_tilePanel->setVisible(t == Tool::TilePaint);

    const char* names[] = {"Tile Paint", "Tile Pick", "Rect Select", "Stamp Paint",
                            "Place Outpost", "Place Spawnpoint", "Select Object"};
    statusBar()->showMessage(
        QString("Tool: %1").arg(names[int(t)]), 2000);
}

// ---------------------------------------------------------------------------
// View actions

void MainWindow::onToggleGrid(bool checked)
{
    m_view->setShowGrid(checked);
}

void MainWindow::onFitToWindow()
{
    m_view->fitToWindow();
    m_statusZoom->setText(QString("%1%").arg(int(m_view->zoom() * 100)));
}

void MainWindow::onZoomIn()
{
    m_view->setZoom(m_view->zoom() * 1.25);
    m_statusZoom->setText(QString("%1%").arg(int(m_view->zoom() * 100)));
}

void MainWindow::onZoomOut()
{
    m_view->setZoom(m_view->zoom() / 1.25);
    m_statusZoom->setText(QString("%1%").arg(int(m_view->zoom() * 100)));
}

// ---------------------------------------------------------------------------
// Status bar callbacks

void MainWindow::onTileHovered(int tx, int ty, int tileId)
{
    QString msg = QString("(%1, %2)  id=%3").arg(tx).arg(ty).arg(tileId);
    if (m_tileset.isValid() && tileId < m_tileset.tileCount()) {
        const TileHeader& h = m_tileset.header(tileId);
        const char* mv =
            h.move_value == 0 ? "road" :
            h.move_value == 1 ? "ground" :
            h.move_value == 4 ? "impassable" :
            h.move_value == 5 ? "water" : "?";
        msg += QString("  [%1]").arg(mv);
    }
    m_statusTile->setText(msg);
    m_statusZoom->setText(QString("%1%").arg(int(m_view->zoom() * 100)));
}

void MainWindow::onMapModified()
{
    if (!m_modified) { m_modified = true; updateTitle(); }
    m_undoAct->setEnabled(m_view->canUndo());
    m_redoAct->setEnabled(m_view->canRedo());
    m_minimap->setMap(&m_view->map());
    m_minimap->rebuildImage();
}

// ---------------------------------------------------------------------------
// Object callbacks

void MainWindow::onObjectSelectionChanged(int idx)
{
    if (idx < 0 || idx >= int(m_view->map().objects.size())) {
        m_statusObj->clear();
        return;
    }
    const auto& obj = m_view->map().objects[size_t(idx)];
    if (obj.type == "outpost")
        m_statusObj->setText(QString("Outpost: %1 (%2, %3)").arg(obj.name).arg(obj.x).arg(obj.y));
    else
        m_statusObj->setText(QString("Spawn point (%1, %2)").arg(obj.x).arg(obj.y));
}

void MainWindow::onObjectActivated(int idx)
{
    if (idx < 0 || idx >= int(m_view->map().objects.size())) return;
    const auto& obj = m_view->map().objects[size_t(idx)];
    if (obj.type != "outpost") return;

    bool ok = false;
    const QString newName = QInputDialog::getText(
        this, "Rename Outpost", "Outpost name:",
        QLineEdit::Normal, obj.name, &ok);
    if (!ok || newName == obj.name) return;

    m_view->applyCommand(
        std::make_unique<RenameObject>(idx, obj.name, newName));
    onObjectSelectionChanged(idx);
}

// ---------------------------------------------------------------------------
// Viewport / minimap sync

void MainWindow::onViewportChanged(const QRectF& tileRect)
{
    m_minimap->setViewportRect(tileRect);
}

void MainWindow::onMinimapPan(QPointF tilePt)
{
    // Pan MapView so `tilePt` is centred in the viewport
    const double px = tilePt.x() * MapView::TILE_SIZE;
    const double py = tilePt.y() * MapView::TILE_SIZE;
    const int newPanX = int(m_view->width()  / 2.0 - px * m_view->zoom());
    const int newPanY = int(m_view->height() / 2.0 - py * m_view->zoom());
    // Trick: temporarily adjust the internal pan via a zero-distance zoom call
    // (MapView doesn't expose setPan, so we use fitToWindow math inline here)
    // Instead, expose a simple slot:
    // Actually just call setZoom which doesn't pan—use a workaround via
    // re-implementing pan. We'll add a panToTile helper to MapView.
    // For now use the public API: fit, then zoom back.
    // A cleaner approach: just add setPan to MapView.
    Q_UNUSED(newPanX); Q_UNUSED(newPanY);
    // We need MapView::setPan — add it.
    m_view->panToTile(tilePt);
}
