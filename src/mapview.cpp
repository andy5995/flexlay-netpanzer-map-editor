#include "mapview.h"
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QInputDialog>
#include <algorithm>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Construction

MapView::MapView(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

// ---------------------------------------------------------------------------
// Map / tileset

void MapView::setMap(const Map& map)
{
    m_map   = map;
    m_pan   = QPoint(0, 0);
    m_zoom  = 1.0;
    m_undo.clear();
    m_redo.clear();
    m_currentStroke.reset();
    m_strokeTiles.clear();
    m_selectedObj  = -1;
    m_draggingObj  = false;
    m_panning      = false;
    update();
    emitViewportChanged();
}

void MapView::setTileset(const Tileset* ts)
{
    m_tileset    = ts;
    m_atlasPixmap = QPixmap();
    update();
}

// ---------------------------------------------------------------------------
// Zoom / pan

void MapView::setZoom(double z)
{
    m_zoom = std::clamp(z, 0.05, 16.0);
    update();
    emitViewportChanged();
}

void MapView::fitToWindow()
{
    if (!m_map.isValid()) return;
    const double ws = double(width())  / double(m_map.width  * TILE_SIZE);
    const double hs = double(height()) / double(m_map.height * TILE_SIZE);
    m_zoom = std::min(ws, hs);
    m_pan  = QPoint((width()  - int(m_map.width  * TILE_SIZE * m_zoom)) / 2,
                    (height() - int(m_map.height * TILE_SIZE * m_zoom)) / 2);
    update();
    emitViewportChanged();
}

// ---------------------------------------------------------------------------
// Tool

void MapView::setTool(Tool t)
{
    m_tool = t;
    emit toolChanged(t);
    commitStroke();
    m_draggingObj     = false;
    m_selectedObj     = -1;
    m_stampPainting   = false;
    m_stampHoverTile  = QPoint(-1, -1);
    m_outpostHoverTile  = QPoint(-1, -1);
    m_ellipseActive     = false;
    m_rectOutlineActive = false;
    emit objectSelectionChanged(-1);
    update();

    switch (t) {
    case Tool::TilePick:        setCursor(Qt::PointingHandCursor); break;
    case Tool::EllipsePaint:
    case Tool::RectOutline:
    case Tool::RectSelect:
    case Tool::RectFill:        setCursor(Qt::CrossCursor); break;
    case Tool::StampPaint:      setCursor(Qt::CrossCursor); break;
    case Tool::SelectObject:    setCursor(Qt::ArrowCursor); break;
    case Tool::PlaceOutpost:
    case Tool::PlaceSpawnpoint: setCursor(Qt::CrossCursor); break;
    default:                    setCursor(Qt::ArrowCursor); break;
    }
}

// ---------------------------------------------------------------------------
// Coordinates

QPointF MapView::widgetToMapPx(QPoint wpos) const
{
    return (QPointF(wpos) - QPointF(m_pan)) / m_zoom;
}

bool MapView::widgetToTile(QPoint wpos, int& tx, int& ty) const
{
    if (!m_map.isValid()) return false;
    const QPointF mp = widgetToMapPx(wpos);
    tx = int(mp.x()) / TILE_SIZE;
    ty = int(mp.y()) / TILE_SIZE;
    if (mp.x() < 0) tx--;   // correct negative floor
    if (mp.y() < 0) ty--;
    return tx >= 0 && ty >= 0 && tx < m_map.width && ty < m_map.height;
}

int MapView::objectAt(QPoint wpos) const
{
    if (m_map.objects.empty()) return -1;
    const QPointF mp = widgetToMapPx(wpos);
    // Enforce a minimum hit radius of 8 screen pixels so objects remain
    // clickable at low zoom levels.
    const double hitR = std::max(TILE_SIZE * 0.6, 8.0 / m_zoom);
    // Iterate in reverse so topmost-drawn object wins
    for (int i = int(m_map.objects.size()) - 1; i >= 0; --i) {
        const auto& obj = m_map.objects[size_t(i)];
        const double cx = (obj.x + 0.5) * TILE_SIZE;
        const double cy = (obj.y + 0.5) * TILE_SIZE;
        const double dx = mp.x() - cx;
        const double dy = mp.y() - cy;
        if (dx*dx + dy*dy <= hitR*hitR) return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Command stack

void MapView::pushCommand(std::unique_ptr<Command> cmd)
{
    // cmd already applied — just record for undo
    m_undo.push_back(std::move(cmd));
    m_redo.clear();
}

void MapView::applyCommand(std::unique_ptr<Command> cmd)
{
    cmd->apply(m_map);
    m_undo.push_back(std::move(cmd));
    m_redo.clear();
    update();
    emit mapModified();
}

void MapView::undo()
{
    if (m_undo.empty()) return;
    auto cmd = std::move(m_undo.back());
    m_undo.pop_back();
    cmd->revert(m_map);
    m_redo.push_back(std::move(cmd));
    m_selectedObj = -1;
    emit objectSelectionChanged(-1);
    update();
    emit mapModified();
}

void MapView::redo()
{
    if (m_redo.empty()) return;
    auto cmd = std::move(m_redo.back());
    m_redo.pop_back();
    cmd->apply(m_map);
    m_undo.push_back(std::move(cmd));
    m_selectedObj = -1;
    emit objectSelectionChanged(-1);
    update();
    emit mapModified();
}

// ---------------------------------------------------------------------------
// Rect selection / stamp

Stamp MapView::captureSelection() const
{
    Stamp s;
    if (m_selection.isNull() || !m_map.isValid()) return s;
    const int x0 = std::max(0, m_selection.x());
    const int y0 = std::max(0, m_selection.y());
    const int x1 = std::min(m_map.width  - 1, m_selection.right());
    const int y1 = std::min(m_map.height - 1, m_selection.bottom());
    s.width  = x1 - x0 + 1;
    s.height = y1 - y0 + 1;
    s.tiles.resize(size_t(s.width * s.height));
    for (int row = 0; row < s.height; ++row)
        for (int col = 0; col < s.width; ++col)
            s.tiles[size_t(row * s.width + col)] =
                m_map.tiles[size_t((y0 + row) * m_map.width + (x0 + col))];
    return s;
}

void MapView::setCurrentStamp(const Stamp* stamp)
{
    m_currentStamp   = stamp;
    m_stampHoverTile = QPoint(-1, -1);
    update();
}

void MapView::applyStamp(int tx, int ty)
{
    if (!m_currentStamp || !m_map.isValid()) return;
    auto batch = std::make_unique<TileBatch>();
    for (int row = 0; row < m_currentStamp->height; ++row) {
        for (int col = 0; col < m_currentStamp->width; ++col) {
            const int mtx = tx + col;
            const int mty = ty + row;
            if (mtx < 0 || mty < 0 || mtx >= m_map.width || mty >= m_map.height)
                continue;
            const int idx = mty * m_map.width + mtx;
            const uint16_t oldVal = m_map.tiles[size_t(idx)];
            const uint16_t newVal = m_currentStamp->tiles[size_t(row * m_currentStamp->width + col)];
            if (oldVal == newVal) continue;
            m_map.tiles[size_t(idx)] = newVal;
            batch->edits.push_back({idx, oldVal, newVal});
        }
    }
    if (!batch->empty()) {
        pushCommand(std::move(batch));
        update();
        emit mapModified();
    }
}

// ---------------------------------------------------------------------------
// Pan to tile (used by minimap click)

void MapView::panToTile(QPointF tilePt)
{
    const double px = tilePt.x() * TILE_SIZE;
    const double py = tilePt.y() * TILE_SIZE;
    m_pan = QPoint(int(width()  / 2.0 - px * m_zoom),
                   int(height() / 2.0 - py * m_zoom));
    update();
    emitViewportChanged();
}

// ---------------------------------------------------------------------------
// Tile stroke

void MapView::startStroke()
{
    m_currentStroke = std::make_unique<TileBatch>();
    m_strokeTiles.clear();
}

void MapView::addToStroke(int tx, int ty)
{
    if (!m_currentStroke) return;

    const AutotileGroup* grp = nullptr;
    if (m_autotileEnabled && m_autotileSet.isLoaded())
        grp = m_autotileSet.groupForTile(m_selectedTile);

    // 4dir piece mode: resolve to piece top-left, then paint the whole piece.
    if (grp && grp->is4dir && grp->piece_size > 1) {
        auto offIt = grp->tile_to_offset.constFind(m_selectedTile);
        if (offIt == grp->tile_to_offset.constEnd()) return;
        const int px = tx - offIt->first;
        const int py = ty - offIt->second;
        const int pidx = py * m_map.width + px;
        if (m_strokeTiles.contains(pidx)) return;
        addPieceToStroke(px, py, *grp);
        return;
    }

    // blob8 / single-tile mode
    const int idx = ty * m_map.width + tx;
    if (m_strokeTiles.contains(idx)) return;
    m_strokeTiles.insert(idx);

    const uint16_t oldVal = m_map.tiles[size_t(idx)];
    uint16_t newVal = uint16_t(m_selectedTile);

    if (grp) {
        const int bm = AutotileSet::computeBitmask(
            m_map.tiles.data(), m_map.width, m_map.height, tx, ty, *grp);
        const int variant = AutotileSet::tileForBitmask(*grp, bm);
        newVal = uint16_t(variant >= 0 ? variant : m_selectedTile);
    }

    if (oldVal == newVal) return;

    m_map.tiles[size_t(idx)] = newVal;
    m_currentStroke->edits.push_back({idx, oldVal, newVal});

    if (grp)
        updateAutotileNeighbors(tx, ty, *grp);

    update();
    emit mapModified();
}

void MapView::addPieceToStroke(int px, int py, const AutotileGroup& grp)
{
    const int pidx = py * m_map.width + px;
    m_strokeTiles.insert(pidx);

    // Determine intent: the piece type of the currently selected tile.
    const int intentBm = [&] {
        auto it = grp.tile_to_piece_bitmask.constFind(m_selectedTile);
        return (it != grp.tile_to_piece_bitmask.constEnd()) ? it.value() : 15;
    }();

    const int bm = AutotileSet::computePieceBitmask(
        m_map.tiles.data(), m_map.width, m_map.height, px, py, grp.piece_size, grp);
    const AutotileGroup::Piece* piece = AutotileSet::pieceForBitmask(grp, bm, intentBm);
    if (!piece || piece->tiles.empty()) return;

    applyPieceTiles(px, py, *piece);
    updatePieceNeighbors(px, py, grp);
    update();
    emit mapModified();
}

void MapView::applyPieceTiles(int px, int py, const AutotileGroup::Piece& piece)
{
    for (int r = 0; r < piece.h; ++r) {
        for (int c = 0; c < piece.w; ++c) {
            const int tid = piece.tiles[size_t(r * piece.w + c)];
            if (tid < 0) continue;
            const int ntx = px + c;
            const int nty = py + r;
            if (ntx < 0 || ntx >= m_map.width || nty < 0 || nty >= m_map.height) continue;
            const int nidx = nty * m_map.width + ntx;
            const uint16_t oldVal = m_map.tiles[size_t(nidx)];
            const uint16_t newVal = uint16_t(tid);
            if (oldVal == newVal) continue;
            m_map.tiles[size_t(nidx)] = newVal;
            if (m_currentStroke)
                m_currentStroke->edits.push_back({nidx, oldVal, newVal});
        }
    }
}

void MapView::updatePieceNeighbors(int px, int py, const AutotileGroup& grp)
{
    const int ps = grp.piece_size;
    // Cardinal offsets to the 4 adjacent piece positions.
    const int noff[4][2] = {{0,-ps}, {ps,0}, {0,ps}, {-ps,0}};

    QSet<int> visited;
    for (auto& off : noff) {
        const int cx = px + off[0];
        const int cy = py + off[1];
        // Search the expected piece area for any member tile, then trace to top-left.
        for (int r = 0; r < ps; ++r) {
            for (int c = 0; c < ps; ++c) {
                const int nx = cx + c;
                const int ny = cy + r;
                if (nx < 0 || nx >= m_map.width || ny < 0 || ny >= m_map.height) continue;
                const int tid = int(m_map.tiles[size_t(ny * m_map.width + nx)]);
                if (!grp.member_tiles.contains(tid)) continue;
                auto offIt = grp.tile_to_offset.constFind(tid);
                if (offIt == grp.tile_to_offset.constEnd()) goto next_dir;
                {
                    const int npx   = nx - offIt->first;
                    const int npy   = ny - offIt->second;
                    const int npidx = npy * m_map.width + npx;
                    if (visited.contains(npidx)) goto next_dir;
                    visited.insert(npidx);
                    const int nbm = AutotileSet::computePieceBitmask(
                        m_map.tiles.data(), m_map.width, m_map.height,
                        npx, npy, ps, grp);
                    // Use the neighbor's current piece type as intent.
                    auto nIntentIt = grp.tile_to_piece_bitmask.constFind(tid);
                    const int nIntentBm = (nIntentIt != grp.tile_to_piece_bitmask.constEnd())
                                          ? nIntentIt.value() : 15;
                    const AutotileGroup::Piece* np = AutotileSet::pieceForBitmask(grp, nbm, nIntentBm);
                    if (np) applyPieceTiles(npx, npy, *np);
                }
                goto next_dir;
            }
        }
        next_dir:;
    }
}

void MapView::updateAutotileNeighbors(int tx, int ty, const AutotileGroup& grp)
{
    static const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    for (int d = 0; d < 8; ++d) {
        const int nx = tx + dx[d];
        const int ny = ty + dy[d];
        if (nx < 0 || nx >= m_map.width || ny < 0 || ny >= m_map.height) continue;
        const int nidx = ny * m_map.width + nx;
        const uint16_t cur = m_map.tiles[size_t(nidx)];
        if (!grp.member_tiles.contains(int(cur))) continue;
        const int bm = AutotileSet::computeBitmask(
            m_map.tiles.data(), m_map.width, m_map.height, nx, ny, grp);
        const int variant = AutotileSet::tileForBitmask(grp, bm);
        const uint16_t nv = uint16_t(variant >= 0 ? variant : cur);
        if (nv == cur) continue;
        m_map.tiles[size_t(nidx)] = nv;
        if (m_currentStroke)
            m_currentStroke->edits.push_back({nidx, cur, nv});
    }
}

void MapView::commitStroke()
{
    if (m_currentStroke && !m_currentStroke->empty()) {
        pushCommand(std::move(m_currentStroke));
        emit mapModified();
    }
    m_currentStroke.reset();
    m_strokeTiles.clear();
}

// ---------------------------------------------------------------------------
// Object deletion

void MapView::deleteSelectedObject()
{
    if (m_selectedObj < 0 || m_selectedObj >= int(m_map.objects.size())) return;
    const int idx     = m_selectedObj;
    ObjectRef obj     = m_map.objects[size_t(idx)];
    m_selectedObj     = -1;
    emit objectSelectionChanged(-1);
    applyCommand(std::make_unique<RemoveObject>(idx, std::move(obj)));
}

// ---------------------------------------------------------------------------
// Ellipse outline tile set (parametric, sufficient steps to avoid gaps)

std::vector<QPoint> MapView::computeEllipseTiles(QPoint a, QPoint b, int mapW, int mapH)
{
    const double cx = (a.x() + b.x()) / 2.0;
    const double cy = (a.y() + b.y()) / 2.0;
    const double rx = std::abs(b.x() - a.x()) / 2.0;
    const double ry = std::abs(b.y() - a.y()) / 2.0;

    // Approximate perimeter (Ramanujan) to choose step count
    const double h    = ((rx - ry) * (rx - ry)) / ((rx + ry) * (rx + ry) + 1e-9);
    const double peri = M_PI * (rx + ry) * (1 + 3*h / (10 + std::sqrt(4 - 3*h)));
    const int steps   = std::max(8, int(std::ceil(peri * 2)));

    std::vector<bool>   visited(size_t(mapW * mapH), false);
    std::vector<QPoint> result;
    for (int i = 0; i < steps; ++i) {
        const double angle = 2.0 * M_PI * i / steps;
        const int tx = std::clamp(int(std::round(cx + rx * std::cos(angle))), 0, mapW - 1);
        const int ty = std::clamp(int(std::round(cy + ry * std::sin(angle))), 0, mapH - 1);
        const size_t idx = size_t(ty * mapW + tx);
        if (!visited[idx]) {
            visited[idx] = true;
            result.push_back(QPoint(tx, ty));
        }
    }
    return result;
}

// Rectangle outline tile set
std::vector<QPoint> MapView::computeRectOutlineTiles(QPoint a, QPoint b, int mapW, int mapH)
{
    const int x0 = std::clamp(std::min(a.x(), b.x()), 0, mapW - 1);
    const int y0 = std::clamp(std::min(a.y(), b.y()), 0, mapH - 1);
    const int x1 = std::clamp(std::max(a.x(), b.x()), 0, mapW - 1);
    const int y1 = std::clamp(std::max(a.y(), b.y()), 0, mapH - 1);

    std::vector<bool>   visited(size_t(mapW * mapH), false);
    std::vector<QPoint> result;
    auto add = [&](int x, int y) {
        const size_t idx = size_t(y * mapW + x);
        if (!visited[idx]) { visited[idx] = true; result.push_back(QPoint(x, y)); }
    };
    for (int x = x0; x <= x1; ++x) { add(x, y0); add(x, y1); }
    for (int y = y0 + 1; y < y1;  ++y) { add(x0, y); add(x1, y); }
    return result;
}

// ---------------------------------------------------------------------------
// Paint event

void MapView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(40, 40, 40));

    if (!m_map.isValid()) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter,
                   "No map loaded.\nUse File → Open to load a .npm file.");
        return;
    }

    if (m_tileset && m_tileset->isValid() && m_atlasPixmap.isNull())
        m_atlasPixmap = QPixmap::fromImage(m_tileset->atlas(ATLAS_COLS));

    p.save();
    p.translate(m_pan);
    p.scale(m_zoom, m_zoom);

    // Visible tile range
    const QRectF visible = p.transform().inverted().mapRect(QRectF(rect()));
    const int x0 = std::max(0, int(visible.left()   / TILE_SIZE));
    const int y0 = std::max(0, int(visible.top()    / TILE_SIZE));
    const int x1 = std::min(m_map.width  - 1, int(visible.right()  / TILE_SIZE));
    const int y1 = std::min(m_map.height - 1, int(visible.bottom() / TILE_SIZE));

    // Tiles
    if (!m_atlasPixmap.isNull()) {
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const int id = m_map.tiles[size_t(y * m_map.width + x)];
                p.drawPixmap(QRectF(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE),
                             m_atlasPixmap,
                             QRectF(m_tileset->atlasRect(id, ATLAS_COLS)));
            }
    } else {
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const int v = m_map.tiles[size_t(y * m_map.width + x)];
                p.fillRect(QRectF(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE),
                           QColor::fromHsv((v * 37) % 360, 180, 200));
            }
    }

    // Stamp ghost preview
    if (m_tool == Tool::StampPaint && m_currentStamp &&
        m_stampHoverTile.x() >= 0 && !m_atlasPixmap.isNull()) {
        const int tx = m_stampHoverTile.x();
        const int ty = m_stampHoverTile.y();
        p.setOpacity(0.4);
        for (int row = 0; row < m_currentStamp->height; ++row) {
            for (int col = 0; col < m_currentStamp->width; ++col) {
                const int id  = m_currentStamp->tiles[size_t(row * m_currentStamp->width + col)];
                const QRect src = m_tileset->atlasRect(id, ATLAS_COLS);
                p.drawPixmap(
                    QRectF((tx + col) * TILE_SIZE, (ty + row) * TILE_SIZE, TILE_SIZE, TILE_SIZE),
                    m_atlasPixmap, QRectF(src));
            }
        }
        // Blue tint overlay so the preview is clearly distinct from placed tiles
        p.setOpacity(0.25);
        p.fillRect(QRectF(tx * TILE_SIZE, ty * TILE_SIZE,
                          m_currentStamp->width  * TILE_SIZE,
                          m_currentStamp->height * TILE_SIZE),
                   QColor(0, 120, 255));
        p.setOpacity(1.0);
        // Dashed border around the preview region
        QPen pen(QColor(0, 180, 255), 1, Qt::DashLine);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(tx * TILE_SIZE, ty * TILE_SIZE,
                          m_currentStamp->width  * TILE_SIZE,
                          m_currentStamp->height * TILE_SIZE));
    }

    // Rect fill preview
    if (m_rectFilling && !m_rectFillPreview.isNull()) {
        const QRectF fr(m_rectFillPreview.x() * TILE_SIZE,
                        m_rectFillPreview.y() * TILE_SIZE,
                        m_rectFillPreview.width()  * TILE_SIZE,
                        m_rectFillPreview.height() * TILE_SIZE);
        p.setOpacity(0.35);
        p.fillRect(fr, QColor(0, 180, 255));
        p.setOpacity(1.0);
        p.setPen(QPen(QColor(0, 180, 255), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(fr);
    }

    // Selection highlight
    if (!m_selection.isNull()) {
        const QRectF selRect(m_selection.x() * TILE_SIZE,
                             m_selection.y() * TILE_SIZE,
                             m_selection.width()  * TILE_SIZE,
                             m_selection.height() * TILE_SIZE);
        p.setOpacity(0.25);
        p.fillRect(selRect, QColor(255, 220, 0));
        p.setOpacity(1.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 220, 0), 0));
        p.drawRect(selRect);
    }

    // Grid
    if (m_showGrid && m_zoom > 0.2) {
        p.setPen(QPen(QColor(0, 0, 0, 80), 0));
        for (int y = y0; y <= y1 + 1; ++y)
            p.drawLine(QPointF(x0 * TILE_SIZE, y * TILE_SIZE),
                       QPointF((x1 + 1) * TILE_SIZE, y * TILE_SIZE));
        for (int x = x0; x <= x1 + 1; ++x)
            p.drawLine(QPointF(x * TILE_SIZE, y0 * TILE_SIZE),
                       QPointF(x * TILE_SIZE, (y1 + 1) * TILE_SIZE));
    }

    // Objects
    p.setRenderHint(QPainter::Antialiasing);
    for (int i = 0; i < int(m_map.objects.size()); ++i) {
        const auto& obj = m_map.objects[size_t(i)];
        const QPointF centre((obj.x + 0.5) * TILE_SIZE, (obj.y + 0.5) * TILE_SIZE);
        const double r = std::max(TILE_SIZE * 0.45, 5.0 / m_zoom);

        const bool selected = (i == m_selectedObj);
        QColor fill = (obj.type == "outpost") ? QColor(220, 60, 60, 210)
                                              : QColor(60, 100, 220, 210);
        if (selected) fill = fill.lighter(140);

        p.setBrush(fill);
        p.setPen(QPen(selected ? Qt::yellow : Qt::white, selected ? 1.5 : 0.5));
        p.drawEllipse(centre, r, r);

        if (m_zoom >= 0.35) {
            p.setPen(Qt::white);
            QFont f = p.font();
            f.setPixelSize(std::max(6, int(10 / m_zoom)));
            p.setFont(f);
            p.drawText(QRectF(centre.x() - r, centre.y() - r, r * 2, r * 2),
                       Qt::AlignCenter,
                       obj.type == "outpost" ? "O" : "S");
        }
    }

    // Ellipse paint preview
    if (m_tool == Tool::EllipsePaint && m_ellipseActive) {
        const auto tiles = computeEllipseTiles(m_ellipseStart, m_ellipseEnd,
                                               m_map.width, m_map.height);
        p.setOpacity(0.55);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 220, 0));
        for (const QPoint& pt : tiles)
            p.fillRect(QRectF(pt.x() * TILE_SIZE, pt.y() * TILE_SIZE,
                              TILE_SIZE, TILE_SIZE), QColor(255, 220, 0, 180));
        p.setOpacity(1.0);
    }

    // Rect outline paint preview
    if (m_tool == Tool::RectOutline && m_rectOutlineActive) {
        const auto tiles = computeRectOutlineTiles(m_rectOutlineStart, m_rectOutlineEnd,
                                                   m_map.width, m_map.height);
        p.setOpacity(0.55);
        p.setPen(Qt::NoPen);
        for (const QPoint& pt : tiles)
            p.fillRect(QRectF(pt.x() * TILE_SIZE, pt.y() * TILE_SIZE,
                              TILE_SIZE, TILE_SIZE), QColor(255, 220, 0, 180));
        p.setOpacity(1.0);
    }

    // Outpost capture-pad overlay.
    // The game places the pad at marker_center + (224, 48) px with a
    // ±48 x ±32 px capture box (Objective.cpp, occupation_pad_offset).
    auto drawCapturePad = [&](QPointF markerCentre, bool ghost) {
        const QPointF padCentre = markerCentre + QPointF(224, 48);
        const QRectF  padRect(padCentre.x() - 48, padCentre.y() - 32, 96, 64);
        p.setOpacity(ghost ? 0.45 : 0.75);
        p.setBrush(QColor(255, 200, 0, 50));
        p.setPen(QPen(QColor(255, 200, 0), ghost ? 0.5 : 1.0));
        p.drawRect(padRect);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 200, 0, 160), 0.5, Qt::DashLine));
        p.drawLine(markerCentre, padCentre);
        p.setOpacity(1.0);
    };

    // Selected outpost
    if (m_selectedObj >= 0 && m_selectedObj < int(m_map.objects.size())) {
        const auto& obj = m_map.objects[size_t(m_selectedObj)];
        if (obj.type == "outpost")
            drawCapturePad(QPointF((obj.x + 0.5) * TILE_SIZE,
                                   (obj.y + 0.5) * TILE_SIZE), false);
    }

    // Ghost while hovering to place
    if (m_tool == Tool::PlaceOutpost && m_outpostHoverTile.x() >= 0) {
        drawCapturePad(QPointF((m_outpostHoverTile.x() + 0.5) * TILE_SIZE,
                                (m_outpostHoverTile.y() + 0.5) * TILE_SIZE), true);
    }

    // Map border
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(200, 200, 200), 0));
    p.drawRect(QRectF(0, 0,
                      m_map.width  * TILE_SIZE,
                      m_map.height * TILE_SIZE));
    p.restore();
}

// ---------------------------------------------------------------------------
// Wheel zoom (toward cursor)

void MapView::wheelEvent(QWheelEvent* ev)
{
    const double factor = (ev->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
    const QPointF pos  = ev->position();
    const QPointF mapPt = (pos - QPointF(m_pan)) / m_zoom;
    m_zoom = std::clamp(m_zoom * factor, 0.05, 16.0);
    m_pan  = QPoint(int(pos.x() - mapPt.x() * m_zoom),
                    int(pos.y() - mapPt.y() * m_zoom));
    update();
    emitViewportChanged();
    ev->accept();
}

void MapView::resizeEvent(QResizeEvent*)
{
    emitViewportChanged();
}

// ---------------------------------------------------------------------------
// Mouse press

void MapView::mousePressEvent(QMouseEvent* ev)
{
    setFocus();

    if (ev->button() == Qt::MiddleButton) {
        m_panning   = true;
        m_lastMouse = ev->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (ev->button() == Qt::RightButton && m_tool == Tool::StampPaint) {
        m_stampPainting  = false;
        m_stampHoverTile = QPoint(-1, -1);
        emit stampDeselected();
        return;
    }

    if (ev->button() != Qt::LeftButton) return;

    switch (m_tool) {
    case Tool::EllipsePaint: {
        int tx, ty;
        if (widgetToTile(ev->pos(), tx, ty)) {
            m_ellipseStart  = QPoint(tx, ty);
            m_ellipseEnd    = QPoint(tx, ty);
            m_ellipseActive = true;
            update();
        }
        break;
    }
    case Tool::RectOutline: {
        int tx, ty;
        if (widgetToTile(ev->pos(), tx, ty)) {
            m_rectOutlineStart  = QPoint(tx, ty);
            m_rectOutlineEnd    = QPoint(tx, ty);
            m_rectOutlineActive = true;
            update();
        }
        break;
    }
    case Tool::TilePick: {
        int tx, ty;
        if (widgetToTile(ev->pos(), tx, ty)) {
            const int id = m_map.tiles[size_t(ty * m_map.width + tx)];
            m_selectedTile = id;
            emit tilePicked(id);
            setTool(Tool::TilePaint);
        }
        break;
    }
    case Tool::RectSelect: {
        int tx, ty;
        if (widgetToTile(ev->pos(), tx, ty)) {
            m_selecting   = true;
            m_selectStart = QPoint(tx, ty);
            m_selection   = QRect(tx, ty, 1, 1);
            emit selectionChanged(m_selection);
            update();
        }
        break;
    }
    case Tool::RectFill: {
        int tx, ty;
        if (widgetToTile(ev->pos(), tx, ty)) {
            m_rectFilling     = true;
            m_rectFillStart   = QPoint(tx, ty);
            m_rectFillPreview = QRect(tx, ty, 1, 1);
            update();
        }
        break;
    }
    case Tool::StampPaint: {
        int tx, ty;
        m_stampPainting = true;
        if (widgetToTile(ev->pos(), tx, ty))
            applyStamp(tx, ty);
        break;
    }
    case Tool::TilePaint: {
        startStroke();
        int tx, ty;
        if (widgetToTile(ev->pos(), tx, ty))
            addToStroke(tx, ty);
        break;
    }
    case Tool::PlaceOutpost:
    case Tool::PlaceSpawnpoint: {
        int tx, ty;
        if (widgetToTile(ev->pos(), tx, ty)) {
            ObjectRef obj;
            obj.type = (m_tool == Tool::PlaceOutpost) ? "outpost" : "spawnpoint";
            // Count existing objects of this type for default name
            if (obj.type == "outpost") {
                int n = 0;
                for (const auto& o : m_map.objects)
                    if (o.type == "outpost") ++n;
                obj.name = QString("Outpost#%1").arg(n + 1);
            }
            obj.x = tx;
            obj.y = ty;
            applyCommand(std::make_unique<AddObject>(std::move(obj)));
        }
        break;
    }
    case Tool::SelectObject: {
        const int hit = objectAt(ev->pos());
        if (hit != m_selectedObj) {
            m_selectedObj = hit;
            emit objectSelectionChanged(hit);
            update();
        }
        if (hit >= 0) {
            m_draggingObj  = true;
            m_objDragOrigX = m_map.objects[size_t(hit)].x;
            m_objDragOrigY = m_map.objects[size_t(hit)].y;
        }
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// Double-click: activate object (rename dialog in main window)

void MapView::mouseDoubleClickEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) return;
    if (m_tool == Tool::SelectObject) {
        const int hit = objectAt(ev->pos());
        if (hit >= 0)
            emit objectActivated(hit);
    }
}

// ---------------------------------------------------------------------------
// Mouse move

void MapView::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_panning) {
        m_pan += ev->pos() - m_lastMouse;
        m_lastMouse = ev->pos();
        update();
        emitViewportChanged();
        return;
    }

    if (m_tool == Tool::TilePaint && (ev->buttons() & Qt::LeftButton)) {
        int tx, ty;
        if (widgetToTile(ev->pos(), tx, ty))
            addToStroke(tx, ty);
    }

    if (m_tool == Tool::RectSelect && m_selecting && (ev->buttons() & Qt::LeftButton)) {
        int tx, ty;
        if (widgetToTile(ev->pos(), tx, ty)) {
            const int x0 = std::min(m_selectStart.x(), tx);
            const int y0 = std::min(m_selectStart.y(), ty);
            const int x1 = std::max(m_selectStart.x(), tx);
            const int y1 = std::max(m_selectStart.y(), ty);
            m_selection = QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
            emit selectionChanged(m_selection);
            update();
        }
    }

    if (m_tool == Tool::RectFill && m_rectFilling && (ev->buttons() & Qt::LeftButton)) {
        int tx, ty;
        if (widgetToTile(ev->pos(), tx, ty)) {
            const int x0 = std::min(m_rectFillStart.x(), tx);
            const int y0 = std::min(m_rectFillStart.y(), ty);
            const int x1 = std::max(m_rectFillStart.x(), tx);
            const int y1 = std::max(m_rectFillStart.y(), ty);
            m_rectFillPreview = QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
            update();
        }
    }

    if (m_tool == Tool::EllipsePaint && m_ellipseActive && (ev->buttons() & Qt::LeftButton)) {
        int tx, ty;
        if (widgetToTile(ev->pos(), tx, ty)) {
            const QPoint newEnd(tx, ty);
            if (newEnd != m_ellipseEnd) {
                m_ellipseEnd = newEnd;
                update();
            }
        }
    }

    if (m_tool == Tool::RectOutline && m_rectOutlineActive && (ev->buttons() & Qt::LeftButton)) {
        int tx, ty;
        if (widgetToTile(ev->pos(), tx, ty)) {
            const QPoint newEnd(tx, ty);
            if (newEnd != m_rectOutlineEnd) {
                m_rectOutlineEnd = newEnd;
                update();
            }
        }
    }

    if (m_tool == Tool::PlaceOutpost) {
        int tx, ty;
        const QPoint newHover = widgetToTile(ev->pos(), tx, ty)
                                ? QPoint(tx, ty) : QPoint(-1, -1);
        if (newHover != m_outpostHoverTile) {
            m_outpostHoverTile = newHover;
            update();
        }
    }

    if (m_tool == Tool::StampPaint) {
        int tx, ty;
        const QPoint newHover = widgetToTile(ev->pos(), tx, ty)
                                ? QPoint(tx, ty) : QPoint(-1, -1);
        if (newHover != m_stampHoverTile) {
            m_stampHoverTile = newHover;
            update();
            if (m_stampPainting && newHover.x() >= 0)
                applyStamp(tx, ty);
        }
    }

    if (m_tool == Tool::SelectObject && m_draggingObj &&
        m_selectedObj >= 0 && (ev->buttons() & Qt::LeftButton)) {
        int tx, ty;
        if (widgetToTile(ev->pos(), tx, ty)) {
            m_map.objects[size_t(m_selectedObj)].x = tx;
            m_map.objects[size_t(m_selectedObj)].y = ty;
            update();
        }
    }

    // Status bar hover info
    int tx, ty;
    if (widgetToTile(ev->pos(), tx, ty) && m_map.isValid()) {
        const int id = m_map.tiles[size_t(ty * m_map.width + tx)];
        emit tileHovered(tx, ty, id);
    }
}

// ---------------------------------------------------------------------------
// Mouse release

void MapView::mouseReleaseEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::MiddleButton) {
        m_panning = false;
        setCursor(m_tool == Tool::PlaceOutpost || m_tool == Tool::PlaceSpawnpoint
                  ? Qt::CrossCursor : Qt::ArrowCursor);
        return;
    }

    if (ev->button() == Qt::LeftButton) {
        m_stampPainting = false;
        if (m_tool == Tool::EllipsePaint && m_ellipseActive) {
            m_ellipseActive = false;
            const auto tiles = computeEllipseTiles(m_ellipseStart, m_ellipseEnd,
                                                   m_map.width, m_map.height);
            startStroke();
            for (const QPoint& pt : tiles)
                addToStroke(pt.x(), pt.y());
            commitStroke();
            update();
        }

        if (m_tool == Tool::RectOutline && m_rectOutlineActive) {
            m_rectOutlineActive = false;
            const auto tiles = computeRectOutlineTiles(m_rectOutlineStart, m_rectOutlineEnd,
                                                       m_map.width, m_map.height);
            startStroke();
            for (const QPoint& pt : tiles)
                addToStroke(pt.x(), pt.y());
            commitStroke();
            update();
        }

        if (m_tool == Tool::TilePaint)
            commitStroke();

        if (m_tool == Tool::RectSelect && m_selecting) {
            m_selecting = false;
            // selection already updated in mouseMoveEvent
        }

        if (m_tool == Tool::RectFill && m_rectFilling) {
            m_rectFilling = false;
            const QRect r = m_rectFillPreview.intersected(
                QRect(0, 0, m_map.width, m_map.height));
            if (r.isValid() && m_map.isValid()) {
                startStroke();
                for (int y = r.top(); y <= r.bottom(); ++y)
                    for (int x = r.left(); x <= r.right(); ++x)
                        addToStroke(x, y);
                commitStroke();
            }
            m_rectFillPreview = QRect();
            update();
        }

        if (m_tool == Tool::SelectObject && m_draggingObj && m_selectedObj >= 0) {
            const auto& obj = m_map.objects[size_t(m_selectedObj)];
            if (obj.x != m_objDragOrigX || obj.y != m_objDragOrigY) {
                // Record the move: revert map first, then apply via command
                const int newX = obj.x, newY = obj.y;
                m_map.objects[size_t(m_selectedObj)].x = m_objDragOrigX;
                m_map.objects[size_t(m_selectedObj)].y = m_objDragOrigY;
                applyCommand(std::make_unique<MoveObject>(
                    m_selectedObj, m_objDragOrigX, m_objDragOrigY, newX, newY));
            }
            m_draggingObj = false;
        }
    }
}

// ---------------------------------------------------------------------------
// Leave

void MapView::leaveEvent(QEvent*)
{
    commitStroke();
    m_draggingObj = false;
    if (m_tool == Tool::StampPaint && m_stampHoverTile.x() >= 0) {
        m_stampHoverTile = QPoint(-1, -1);
        update();
    }
    if (m_tool == Tool::PlaceOutpost && m_outpostHoverTile.x() >= 0) {
        m_outpostHoverTile = QPoint(-1, -1);
        update();
    }
}

// ---------------------------------------------------------------------------
// Keyboard

void MapView::keyPressEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Escape && m_tool == Tool::StampPaint) {
        emit stampDeselected();
        return;
    }
    if (ev->key() == Qt::Key_Delete || ev->key() == Qt::Key_Backspace) {
        if (m_tool == Tool::SelectObject)
            deleteSelectedObject();
    }
}

// ---------------------------------------------------------------------------
// Context menu (right-click)

void MapView::contextMenuEvent(QContextMenuEvent* ev)
{
    if (m_tool == Tool::StampPaint) {
        m_stampPainting = false;
        emit stampDeselected();
        return;
    }

    const int hit = objectAt(ev->pos());
    if (hit < 0) return;

    // Select the hit object so it's highlighted
    if (hit != m_selectedObj) {
        m_selectedObj = hit;
        emit objectSelectionChanged(hit);
        update();
    }

    QMenu menu(this);
    const auto& obj = m_map.objects[size_t(hit)];
    menu.setTitle(obj.type == "outpost" ? obj.name : "Spawn point");

    if (obj.type == "outpost") {
        QAction* rename = menu.addAction("Rename…");
        connect(rename, &QAction::triggered, this, [this, hit]() {
            emit objectActivated(hit);
        });
        menu.addSeparator();
    }

    QAction* del = menu.addAction("Delete");
    connect(del, &QAction::triggered, this, [this]() {
        deleteSelectedObject();
    });

    menu.exec(ev->globalPos());
}

// ---------------------------------------------------------------------------
// Viewport signal

void MapView::emitViewportChanged()
{
    if (!m_map.isValid() || m_zoom <= 0) return;
    const double invZoom = 1.0 / m_zoom;
    const double tw = width()  * invZoom / TILE_SIZE;
    const double th = height() * invZoom / TILE_SIZE;
    const double tx = -m_pan.x() * invZoom / TILE_SIZE;
    const double ty = -m_pan.y() * invZoom / TILE_SIZE;
    emit viewportChanged(QRectF(tx, ty, tw, th));
}
