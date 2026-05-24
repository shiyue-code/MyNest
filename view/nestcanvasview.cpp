#include "nestcanvasview.h"

#include "shapes/s_box.hpp"
#include "shapes/utiltool.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

NestCanvasView::NestCanvasView(QWidget* parent)
    : KWCtrlView(parent)
{
    m_clrBackGround = QColor("#F8FAFC");
    m_bShowTick = false;
    m_bshowOrigin = false;
    m_bShowCross = false;
}

void NestCanvasView::beginNest(const Polyline& stock, const S_Shape2D::NestScene& scene)
{
    this->stock = stock;
    this->scene = scene;
    placed.clear();
    candidates.clear();
    nfps.clear();
    currentPiece.clear();
    utilization = 0;
    ResetView();
    update();
}

void NestCanvasView::addPiece(const Polyline& piece, int pieceIndex, double util)
{
    VisualPiece visual;
    visual.polygon = piece;
    visual.pieceIndex = pieceIndex;
    visual.color = colorForPiece(pieceIndex);
    visual.label = labelForPiece(pieceIndex);
    placed.push_back(visual);
    candidates.clear();
    nfps.clear();
    currentPiece.clear();
    utilization = util;
    update();
}

void NestCanvasView::setPieces(const std::vector<Polyline>& pieces, double util)
{
    std::vector<int> previousPieceIndices;
    previousPieceIndices.reserve(placed.size());
    for (const auto& piece : placed)
        previousPieceIndices.push_back(piece.pieceIndex);

    placed.clear();
    for (int i = 0; i < static_cast<int>(pieces.size()); ++i) {
        int pieceIndex = (i < static_cast<int>(previousPieceIndices.size())) ? previousPieceIndices[i] : i;
        VisualPiece visual;
        visual.polygon = pieces[i];
        visual.pieceIndex = pieceIndex;
        visual.color = colorForPiece(pieceIndex);
        visual.label = labelForPiece(pieceIndex);
        placed.push_back(visual);
    }
    utilization = util;
    update();
}

void NestCanvasView::showCandidates(const std::vector<Polyline>& candidates,
                                    const std::vector<Polyline>& nfps,
                                    const Polyline& currentPiece)
{
    this->candidates = candidates;
    this->nfps = nfps;
    this->currentPiece = currentPiece;
    update();
}

int NestCanvasView::placedSize() const
{
    return static_cast<int>(placed.size());
}

double NestCanvasView::utilizationValue() const
{
    return utilization;
}

void NestCanvasView::ResetView()
{
    S_Shape2D::Box2D box;
    auto append = [&box](const Polyline& poly) {
        for (const auto& pt : poly)
            box.append(pt);
    };

    append(stock);
    for (const auto& visual : placed)
        append(visual.polygon);
    for (const auto& poly : candidates)
        append(poly);
    for (const auto& poly : nfps)
        append(poly);
    append(currentPiece);

    if (!box) {
        box.append({ -100, -100 });
        box.append({ 100, 100 });
    }

    m_rect.setCoords(box.left(), box.top(), box.right(), box.bottom());
    m_fScale = 1.0f;
    m_dViewW = std::max(1.0, m_rect.width() * 0.56);
    m_dViewH = std::max(1.0, m_rect.height() * 0.56);

    const double aspect = height() == 0 ? static_cast<double>(width())
                                        : static_cast<double>(width()) / static_cast<double>(height());
    m_dXTrans = -(m_rect.left() + m_rect.right()) / 2.0f;
    m_dYTrans = -(m_rect.top() + m_rect.bottom()) / 2.0f;

    if (m_dViewW < m_dViewH * aspect)
        m_dViewW = m_dViewH * aspect;
    else
        m_dViewH = m_dViewW / aspect;

    KWCtrlView::ResetView();
    update();
}

void NestCanvasView::DrawGLSence(QPainter& painter)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    bool isDark = palette().color(QPalette::Window).lightness() < 128;
    painter.fillRect(rect(), palette().color(QPalette::Window));

    if (!stock.empty())
        drawPolygon(painter, stock, QColor(100, 116, 139, 22), QColor("#94A3B8"), 2.0);

    int idx = 0;
    for (const auto& visual : placed) {
        if (visual.polygon.empty())
            continue;

        QColor fill = visual.color;
        fill.setAlpha(78);
        drawPolygon(painter, visual.polygon, fill, visual.color.darker(135), 2.0);
        drawPoint(painter, visual.polygon[0], visual.color.darker(150), 4.0);

        S_Shape2D::Box2D box = S_Shape2D::calcBoundingBox(visual.polygon);
        QPointF labelPoint(box.center().x, box.center().y);
        View2Scr(labelPoint);
        painter.setPen(isDark ? QColor("#F1F5F9") : QColor("#0F172A"));
        painter.drawText(labelPoint, QString::number(++idx));
    }

    for (const auto& nfp : nfps)
        drawPolygon(painter, nfp, Qt::NoBrush, QColor("#E11D48"), 1.4, Qt::DashLine);

    for (const auto& cand : candidates) {
        if (!cand.empty())
            drawPoint(painter, cand[0], QColor("#16A34A"), 2.8);
    }
    for (const auto& cand : candidates)
        drawPolygon(painter, cand, Qt::NoBrush, QColor(22, 163, 74, 90), 1.0);

    if (!currentPiece.empty()) {
        drawPoint(painter, currentPiece[0], QColor("#F97316"), 5.0);
        drawPolygon(painter, currentPiece, QColor(249, 115, 22, 32), QColor("#F97316"), 2.0);
    }

    painter.restore();
}

QPointF NestCanvasView::toScreenPoint(const Polyline::Point& pt)
{
    QPointF screenPoint(pt.x, pt.y);
    View2Scr(screenPoint);
    return screenPoint;
}

QPainterPath NestCanvasView::makePath(const Polyline& poly)
{
    QPainterPath path;
    if (poly.empty())
        return path;

    path.moveTo(toScreenPoint(poly[0]));
    for (Polyline::size_type i = 1; i < poly.size(); ++i)
        path.lineTo(toScreenPoint(poly[i]));
    path.closeSubpath();
    return path;
}

void NestCanvasView::drawPolygon(QPainter& painter, const Polyline& poly, const QBrush& fill,
                                 const QColor& stroke, qreal width, Qt::PenStyle style)
{
    if (poly.size() < 2)
        return;

    QPen pen(stroke, width, style);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(fill);
    painter.drawPath(makePath(poly));
}

void NestCanvasView::drawPoint(QPainter& painter, const Polyline::Point& pt, const QColor& color, qreal radius)
{
    const QPointF center = toScreenPoint(pt);
    painter.setPen(QPen(Qt::white, 1.0));
    painter.setBrush(color);
    painter.drawEllipse(center, radius, radius);
}

QColor NestCanvasView::colorForPiece(int pieceIndex) const
{
    if (pieceIndex >= 0 && pieceIndex < static_cast<int>(scene.pieces.size())) {
        const auto& piece = scene.pieces[pieceIndex];
        if (const auto* prototype = S_Shape2D::findPrototype(scene, piece.prototypeId))
            return prototype->color;
    }

    static const QColor fallback[] = {
        QColor("#4C78A8"),
        QColor("#F58518"),
        QColor("#54A24B"),
        QColor("#E45756"),
        QColor("#72B7B2"),
        QColor("#B279A2")
    };
    return fallback[std::max(0, pieceIndex) % (sizeof(fallback) / sizeof(fallback[0]))];
}

QString NestCanvasView::labelForPiece(int pieceIndex) const
{
    if (pieceIndex >= 0 && pieceIndex < static_cast<int>(scene.pieces.size())) {
        const auto& piece = scene.pieces[pieceIndex];
        if (const auto* prototype = S_Shape2D::findPrototype(scene, piece.prototypeId))
            return prototype->name;
    }
    return QString("P%1").arg(pieceIndex + 1);
}
