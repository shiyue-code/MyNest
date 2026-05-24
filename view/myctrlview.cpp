#include "myctrlview.h"

#include "shapes/s_point.hpp"
#include "shapes/s_polyline.hpp"
#include "shapes/utiltool.h"

#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QString>
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

using namespace S_Shape2D;

MyCtrlView::MyCtrlView(QWidget* parent)
    : KWCtrlView(parent)
{
    m_clrBackGround = QColor("#F8FAFC");
    m_bShowTick = false;
    m_bshowOrigin = false;
    m_bShowCross = false;

    metricOverlay = new QLabel(this);
    metricOverlay->setObjectName("metricOverlay");
    metricOverlay->setTextFormat(Qt::RichText);
    metricOverlay->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    metricOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    metricOverlay->setStyleSheet(
        "QLabel#metricOverlay {"
        "background-color: rgba(255, 255, 255, 214);"
        "border: 1px solid #CBD5E1;"
        "border-radius: 8px;"
        "padding: 8px 10px;"
        "color: #0F172A;"
        "font-size: 12px;"
        "}"
    );
    metricOverlay->hide();
}

void MyCtrlView::ResetView()
{
    Box2D box;
    auto appendPolyline = [&box](const Polyline& poly) {
        for (const auto& pt : poly)
            box.append(pt);
    };

    appendPolyline(fixedPolygon);
    appendPolyline(movingPolygon);
    appendPolyline(pTmp);
    for (const auto& nfp : nfps)
        appendPolyline(nfp);
    if (!movingPolygon.empty()) {
        const Point movingRef = movingPolygon[0];
        for (const auto& nfp : nfps) {
            for (const auto& refPoint : nfp) {
                for (const auto& movingPoint : movingPolygon)
                    box.append(refPoint + (movingPoint - movingRef));
            }
        }
    }
    for (const auto& poly : nestPoly)
        appendPolyline(poly);
    for (const auto& poly : nestPoly2)
        appendPolyline(poly);

    if (!box) {
        if (hasDefaultViewBounds) {
            box.append({ 0, 0 });
            box.append({ defaultViewWidth, defaultViewHeight });
        } else {
            box.append({ -200, -200 });
            box.append({ 200, 200 });
        }
    }

    m_rect.setCoords(box.left(), box.top(), box.right(), box.bottom());

    double aspect;

    m_fScale = 1.0f;
    m_dViewW = m_rect.width() * 0.62;
    m_dViewW = (m_dViewW < 1e-6) ? 1 : m_dViewW;

    m_dViewH = m_rect.height() * 0.62;
    m_dViewH = (m_dViewH < 1e-6) ? 1 : m_dViewH;

    if (this->height() == 0) {
        aspect = (GLdouble)this->width();
    } else {
        aspect = (GLdouble)this->width() / (GLdouble)this->height();
    }

    const bool reserveOverlaySpace = displayLayer == DisplayLayer::FixedOnly
                                  || displayLayer == DisplayLayer::MovingOnly;

    m_dXTrans = -(m_rect.left() + m_rect.right()) / 2.0f;
    m_dYTrans = -(m_rect.top() + m_rect.bottom()) / 2.0f;

    if (m_dViewW < m_dViewH * aspect)
        m_dViewW = m_dViewH * aspect;
    else
        m_dViewH = m_dViewW / aspect;

    if (reserveOverlaySpace && std::max(m_dViewW, m_dViewH) < 100.0) {
        if (m_dViewW >= m_dViewH) {
            m_dViewW = 100.0;
            m_dViewH = m_dViewW / aspect;
        } else {
            m_dViewH = 100.0;
            m_dViewW = m_dViewH * aspect;
        }
    }

    const double overlayWorldOffset = reserveOverlaySpace ? m_dViewW * 0.32 : 0.0;
    m_dXTrans += overlayWorldOffset;

    KWCtrlView::ResetView();
    update();
}

void MyCtrlView::setMode(int m)
{
    if (mode == m)
        return;

    switch (m) {
    case View: {
    } break;
    case DrawPolyline1: {
        pTmp.clear();
    } break;
    case DrawPolyline2: {
        pTmp.clear();
    } break;
    default:
        break;
    }
    mode = m;
}

void MyCtrlView::DrawGLSence(QPainter& painter)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    bool isDark = palette().color(QPalette::Window).lightness() < 128;
    painter.fillRect(rect(), palette().color(QPalette::Window));

    auto toScreen = [this](const Point& pt) {
        QPointF screenPoint(pt.x, pt.y);
        View2Scr(screenPoint);
        return screenPoint;
    };

    auto makePath = [&](const Polyline& poly, bool closePath = true) {
        QPainterPath path;
        if (poly.empty())
            return path;

        path.moveTo(toScreen(poly[0]));
        for (Polyline::size_type i = 1; i < poly.size(); ++i)
            path.lineTo(toScreen(poly[i]));
        if (closePath)
            path.closeSubpath();
        return path;
    };

    auto niceGridStep = [](double rawStep) {
        if (rawStep <= 0.0)
            return 50.0;

        const double exponent = std::pow(10.0, std::floor(std::log10(rawStep)));
        const double factor = rawStep / exponent;
        if (factor <= 2.0)
            return 2.0 * exponent;
        if (factor <= 5.0)
            return 5.0 * exponent;
        return 10.0 * exponent;
    };

    double currentGridStep = 50.0;
    auto drawGrid = [&]() {
        QPointF topLeft(0.0, 0.0);
        QPointF bottomRight(width(), height());
        Scr2View(topLeft);
        Scr2View(bottomRight);

        const double left = std::min(topLeft.x(), bottomRight.x());
        const double right = std::max(topLeft.x(), bottomRight.x());
        const double top = std::min(topLeft.y(), bottomRight.y());
        const double bottom = std::max(topLeft.y(), bottomRight.y());
        const double span = std::max(right - left, bottom - top);
        const double step = niceGridStep(span / 18.0);
        currentGridStep = step;

        QPen minorPen(isDark ? QColor("#334155") : QColor("#E5EAF0"), 1);
        QPen majorPen(isDark ? QColor("#475569") : QColor("#CBD5E1"), 1);
        QPen axisXPen(QColor("#EF4444"), 1.4);
        QPen axisYPen(QColor("#2563EB"), 1.4);

        const int firstX = static_cast<int>(std::floor(left / step)) - 1;
        const int lastX = static_cast<int>(std::ceil(right / step)) + 1;
        for (int i = firstX; i <= lastX; ++i) {
            const double x = i * step;
            QPointF p1(x, top);
            QPointF p2(x, bottom);
            View2Scr(p1);
            View2Scr(p2);
            painter.setPen(std::abs(x) < 1e-9 ? axisYPen : (i % 5 == 0 ? majorPen : minorPen));
            painter.drawLine(p1, p2);
        }

        const int firstY = static_cast<int>(std::floor(top / step)) - 1;
        const int lastY = static_cast<int>(std::ceil(bottom / step)) + 1;
        for (int i = firstY; i <= lastY; ++i) {
            const double y = i * step;
            QPointF p1(left, y);
            QPointF p2(right, y);
            View2Scr(p1);
            View2Scr(p2);
            painter.setPen(std::abs(y) < 1e-9 ? axisXPen : (i % 5 == 0 ? majorPen : minorPen));
            painter.drawLine(p1, p2);
        }
    };

    const bool showFullScene = displayLayer == DisplayLayer::FullScene;
    const bool showFixed = showFullScene || displayLayer == DisplayLayer::FixedOnly;
    const bool showMoving = showFullScene || displayLayer == DisplayLayer::MovingOnly;

    auto drawStockBounds = [&]() {
        if (!hasDefaultViewBounds || showFullScene)
            return;

        Polyline stockBounds;
        stockBounds.add({0, 0});
        stockBounds.add({defaultViewWidth, 0});
        stockBounds.add({defaultViewWidth, defaultViewHeight});
        stockBounds.add({0, defaultViewHeight});

        painter.setPen(QPen(QColor("#94A3B8"), 1.8, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(QColor(100, 116, 139, 18));
        painter.drawPath(makePath(stockBounds));

        QPointF labelPoint(defaultViewWidth, defaultViewHeight);
        View2Scr(labelPoint);
        painter.setPen(QColor("#475569"));
        painter.drawText(labelPoint + QPointF(-116, 18),
                         QString::fromWCharArray(L"\u677F\u6750 %1 x %2")
                             .arg(defaultViewWidth, 0, 'f', 0)
                             .arg(defaultViewHeight, 0, 'f', 0));
    };

    auto drawReferencePoint = [&](const Point& pt, const QColor& color, const QString& text) {
        const QPointF screenPoint = toScreen(pt);
        painter.setPen(QPen(color, 1.6));
        painter.drawLine(screenPoint + QPointF(-7, 0), screenPoint + QPointF(7, 0));
        painter.drawLine(screenPoint + QPointF(0, -7), screenPoint + QPointF(0, 7));
        painter.setBrush(color);
        painter.drawEllipse(screenPoint, pointSize + 1, pointSize + 1);

        painter.setPen(isDark ? QColor("#F1F5F9") : QColor("#111827"));
        painter.drawText(screenPoint + QPointF(10, -10), text);
    };

    auto drawPolygon = [&](const Polyline& poly,
                           const QColor& fill,
                           const QColor& stroke,
                           double strokeWidth,
                           const QString& label,
                           bool highlight = false,
                           bool showReference = true) {
        if (poly.empty())
            return;

        const QPainterPath path = makePath(poly);
        painter.fillPath(path, fill);
        painter.setPen(QPen(stroke, highlight ? strokeWidth + 1.6 : strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);

        const QRectF bounds = path.boundingRect();
        painter.setPen(isDark ? QColor("#F1F5F9") : QColor("#0F172A"));
        painter.drawText(bounds.center(), label);
        if (showReference)
            drawReferencePoint(poly[0], highlight ? QColor("#F59E0B") : stroke, QString("%1 Ref").arg(label));
    };

    auto drawNfp = [&](const Polyline& nfp, int index) {
        if (nfp.empty())
            return;

        QPen pen(QColor("#E11D48"), 2.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(makePath(nfp));
        drawReferencePoint(nfp[0], QColor("#E11D48"), QString("NFP%1").arg(index + 1));
    };

    auto drawNestPolygons = [&](const std::vector<Polyline>& polygons, bool secondary) {
        static const QColor fills[] = {
            QColor(14, 165, 233, 42),
            QColor(34, 197, 94, 42),
            QColor(245, 158, 11, 42),
            QColor(168, 85, 247, 42),
            QColor(236, 72, 153, 42),
        };
        static const QColor strokes[] = {
            QColor("#0284C7"),
            QColor("#16A34A"),
            QColor("#D97706"),
            QColor("#7C3AED"),
            QColor("#DB2777"),
        };

        for (size_t i = 0; i < polygons.size(); ++i) {
            const QColor stroke = secondary ? QColor("#64748B") : strokes[i % 5];
            const QColor fill = secondary ? QColor(100, 116, 139, 30) : fills[i % 5];
            drawPolygon(polygons[i], fill, stroke, 1.6, QString::number(static_cast<int>(i + 1)), false, false);
        }
    };

    drawGrid();
    drawStockBounds();
    if (showFullScene) {
        drawNestPolygons(nestPoly2, true);
        drawNestPolygons(nestPoly, false);
    }

    if (showFixed)
        drawPolygon(fixedPolygon, QColor(37, 99, 235, 42), QColor("#2563EB"), 2.2,
                    QString::fromWCharArray(L"\u57FA\u51C6\u4EF6"), mode == DrawPolyline1);

    Polyline movingPolygonForDraw = animationEnabled ? animationBaseMovingPolygon : movingPolygon;
    if (animationEnabled && !movingPolygonForDraw.empty())
        movingPolygonForDraw.translate(animationRefPoint - movingPolygonForDraw[0]);
    if (showMoving)
        drawPolygon(movingPolygonForDraw, QColor(249, 115, 22, 48), QColor("#EA580C"), 2.2,
                    QString::fromWCharArray(L"\u8FD0\u52A8\u4EF6"), mode == DrawPolyline2);

    if (showFullScene) {
        for (size_t i = 0; i < nfps.size(); ++i)
            drawNfp(nfps[i], static_cast<int>(i));
    }

    const bool showDrawingPreview = (mode == DrawPolyline1 && displayLayer == DisplayLayer::FixedOnly)
                                 || (mode == DrawPolyline2 && displayLayer == DisplayLayer::MovingOnly)
                                 || (showFullScene && (mode == DrawPolyline2 || mode == DrawPolyline1));
    if (showDrawingPreview && !pTmp.empty()) {
        Polyline preview = pTmp;
        preview.add(ptCur);
        painter.setPen(QPen(mode == DrawPolyline1 ? QColor("#2563EB") : QColor("#EA580C"),
                            1.8,
                            Qt::DashLine,
                            Qt::RoundCap,
                            Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(makePath(preview));

        const Point& lastPoint = pTmp[pTmp.size() - 1];
        const Point edge = ptCur - lastPoint;
        const double edgeLength = edge.norm();
        QPointF lengthLabel((ptCur.x + lastPoint.x) * 0.5, (ptCur.y + lastPoint.y) * 0.5);
        View2Scr(lengthLabel);
        painter.setPen(QColor("#0F172A"));
        painter.setBrush(QColor(255, 255, 255, 220));
        const QString lengthText = QString::fromWCharArray(L"\u957F\u5EA6 %1").arg(edgeLength, 0, 'f', 1);
        const QRectF lengthRect(lengthLabel + QPointF(8, -24), QSizeF(86, 22));
        painter.drawRoundedRect(lengthRect, 5, 5);
        painter.drawText(lengthRect, Qt::AlignCenter, lengthText);
    }

    Polyline metricPoly;
    if (showDrawingPreview && !pTmp.empty()) {
        metricPoly = pTmp;
        metricPoly.add(ptCur);
    } else if (displayLayer == DisplayLayer::FixedOnly) {
        metricPoly = fixedPolygon;
    } else if (displayLayer == DisplayLayer::MovingOnly) {
        metricPoly = movingPolygon;
    }
    updateMetricOverlay(metricPoly, currentGridStep);

    if (showFullScene && fixedPolygon.empty() && movingPolygon.empty() && nfps.empty()) {
        painter.setPen(isDark ? QColor("#94A3B8") : QColor("#64748B"));
        painter.drawText(rect(), Qt::AlignCenter,
                         QString::fromWCharArray(L"\u8BA1\u7B97 NFP \u540E\u663E\u793A\u8FD0\u52A8\u9A8C\u8BC1\u52A8\u753B"));
    }

    painter.restore();
}

void MyCtrlView::setPointSize(int size)
{
    pointSize = size;
}

void MyCtrlView::setDisplayLayer(DisplayLayer layer)
{
    if (displayLayer == layer)
        return;

    displayLayer = layer;
    update();
}

void MyCtrlView::setDefaultViewBounds(double width, double height)
{
    defaultViewWidth = std::max(1.0, width);
    defaultViewHeight = std::max(1.0, height);
    hasDefaultViewBounds = true;
    ResetView();
}

void MyCtrlView::setPolylinesChangedCallback(std::function<void()> callback)
{
    polylinesChangedCallback = std::move(callback);
}

void MyCtrlView::updateMetricOverlay(const Polyline& polygon, double gridStep)
{
    if (!metricOverlay)
        return;

    if (displayLayer == DisplayLayer::FullScene) {
        metricOverlay->hide();
        return;
    }

    const QString title = displayLayer == DisplayLayer::FixedOnly
        ? QString::fromWCharArray(L"\u57FA\u51C6\u4EF6\u8F6E\u5ED3")
        : QString::fromWCharArray(L"\u8FD0\u52A8\u4EF6\u8F6E\u5ED3");

    QString text;
    if (polygon.empty()) {
        text = QString("<b>%1</b><br/><span style='color:#64748B;'>%2</span>")
                   .arg(title,
                        QString::fromWCharArray(L"\u672A\u7ED8\u5236\u8F6E\u5ED3"));
    } else {
        Box2D box;
        for (const auto& pt : polygon)
            box.append(pt);

        text = QString("<b>%1</b><br/>"
                       "%2: %3<br/>"
                       "%4: %5&nbsp;&nbsp;&nbsp;%6: %7<br/>"
                       "%8: %9<br/>"
                       "<span style='color:#64748B;'>%10: %11</span>")
                   .arg(title,
                        QString::fromWCharArray(L"\u9876\u70B9"),
                        QString::number(polygon.size()),
                        QString::fromWCharArray(L"\u5BBD"),
                        QString::number(box.width(), 'f', 1),
                        QString::fromWCharArray(L"\u9AD8"),
                        QString::number(box.height(), 'f', 1),
                        QString::fromWCharArray(L"\u9762\u79EF"),
                        QString::number(std::fabs(polygon.area()), 'f', 1),
                        QString::fromWCharArray(L"\u7F51\u683C"),
                        QString::number(gridStep, 'f', gridStep >= 10 ? 0 : 1));
    }

    if (metricOverlayText != text) {
        metricOverlayText = text;
        metricOverlay->setText(text);
        metricOverlay->adjustSize();
    }

    const int maxWidth = std::max(180, width() - 28);
    metricOverlay->setMaximumWidth(maxWidth);
    metricOverlay->move(14, 14);
    metricOverlay->raise();
    metricOverlay->show();
}

void MyCtrlView::setNFPs(const std::vector<MyCtrlView::Polyline>& nfp)
{
    nfps = nfp;
    ResetView();
}

void MyCtrlView::setPolyline(const MyCtrlView::Polyline& fixed, const MyCtrlView::Polyline& moving)
{
    fixedPolygon = fixed;
    movingPolygon = moving;
    stopNfpAnimation();
    ResetView();
    if (polylinesChangedCallback)
        polylinesChangedCallback();
}

void MyCtrlView::startNfpAnimation()
{
    animationPath.clear();
    for (const auto& nfp : nfps) {
        if (nfp.size() >= 3) {
            for (const auto& pt : nfp)
                animationPath.push_back(pt);
            if (animationPath.front() != animationPath.back())
                animationPath.push_back(animationPath.front());
            break;
        }
    }

    if (animationPath.size() < 4 || movingPolygon.empty()) {
        stopNfpAnimation();
        return;
    }

    animationBaseMovingPolygon = movingPolygon;
    animationRefPoint = animationPath[0];
    animationEdgeIndex = 0;
    animationEdgeOffset = 0;
    animationEnabled = true;
    update();
}

void MyCtrlView::stopNfpAnimation()
{
    animationEnabled = false;
    animationPath.clear();
    animationEdgeIndex = 0;
    animationEdgeOffset = 0;
}

void MyCtrlView::advanceNfpAnimation()
{
    if (!animationEnabled || animationPath.size() < 2)
        return;

    double remain = animationStep;
    while (remain > 0) {
        size_t nextIdx = (animationEdgeIndex + 1) % animationPath.size();
        const Point& edgeStart = animationPath[animationEdgeIndex];
        const Point& edgeEnd = animationPath[nextIdx];
        Point edge = edgeEnd - edgeStart;
        double edgeLength = edge.norm();

        if (edgeLength < 1e-9) {
            animationEdgeIndex = nextIdx;
            animationEdgeOffset = 0;
            continue;
        }

        double leftOnEdge = edgeLength - animationEdgeOffset;
        if (remain < leftOnEdge) {
            animationEdgeOffset += remain;
            remain = 0;
        } else {
            remain -= leftOnEdge;
            animationEdgeIndex = nextIdx;
            animationEdgeOffset = 0;
        }
    }

    const Point& curStart = animationPath[animationEdgeIndex];
    const Point& curEnd = animationPath[(animationEdgeIndex + 1) % animationPath.size()];
    Point curEdge = curEnd - curStart;
    double curLength = curEdge.norm();
    if (curLength > 1e-9)
        animationRefPoint = curStart + curEdge * (animationEdgeOffset / curLength);
    else
        animationRefPoint = curStart;

    update();
}

void MyCtrlView::getPolyline(MyCtrlView::Polyline& fixed, MyCtrlView::Polyline& moving)
{
    fixed = fixedPolygon;
    moving = movingPolygon;
}

void MyCtrlView::setNestPoly(const std::vector<MyCtrlView::Polyline>& np)
{
    nestPoly = np;
    ResetView();
}
void MyCtrlView::setNestPoly2(const std::vector<MyCtrlView::Polyline>& np)
{
    nestPoly2 = np;
    ResetView();
}
void MyCtrlView::mouseDoubleClickEvent(QMouseEvent* evt)
{
    if (evt->button() == Qt::LeftButton)
        ResetView();

    return KWCtrlView::mouseDoubleClickEvent(evt);
}

void MyCtrlView::mousePressEvent(QMouseEvent* evt)
{
    if (evt->button() == Qt::LeftButton) {
        switch (mode) {
        case DrawPolyline1: {
            auto pt = evt->localPos();
            Scr2View(pt);
            pTmp.add({ pt.x(), pt.y() });
        } break;
        case DrawPolyline2: {
            auto pt = evt->localPos();
            Scr2View(pt);
            pTmp.add({ pt.x(), pt.y() });
        } break;
        }
    }

    return KWCtrlView::mousePressEvent(evt);
}

void MyCtrlView::mouseReleaseEvent(QMouseEvent* evt)
{
    if (evt->button() == Qt::RightButton) {

        if (!pTmp.empty()) {
            switch (mode) {
            case DrawPolyline1: {
                fixedPolygon = pTmp;
            } break;
            case DrawPolyline2: {
                movingPolygon = pTmp;
            } break;
            }

            pTmp.clear();
            mode = View;
            if (polylinesChangedCallback)
                polylinesChangedCallback();
        }
    }

    return KWCtrlView::mouseReleaseEvent(evt);
}

void MyCtrlView::mouseMoveEvent(QMouseEvent* evt)
{
    auto pt = evt->localPos();
    Scr2View(pt);
    ptCur.x = pt.x();
    ptCur.y = pt.y();

    return KWCtrlView::mouseMoveEvent(evt);
}
