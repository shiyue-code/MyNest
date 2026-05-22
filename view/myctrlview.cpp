#include "myctrlview.h"

#include "shapes/s_point.hpp"
#include "shapes/s_polyline.hpp"
#include "shapes/utiltool.h"

#include <QMouseEvent>
#include <QPainter>
#include <QString>
#include <cmath>
#include <functional>

using namespace S_Shape2D;

MyCtrlView::MyCtrlView(QWidget* parent)
    : KWCtrlView(parent)
{
}

void MyCtrlView::ResetView()
{
    Box2D box;

    box.append({ -200, -200 });
    box.append({ 200, -200 });
    box.append({ 200, 200 });
    box.append({ -200, 200 });

    if (!box) {
        return;
    }

    m_rect.setCoords(box.left(), box.top(), box.right(), box.bottom());

    double aspect;

    m_fScale = 1.0f;
    m_dViewW = m_rect.width() * 0.51;
    m_dViewW = (m_dViewW < 1e-6) ? 1 : m_dViewW;

    m_dViewH = m_rect.height() * 0.51;
    m_dViewH = (m_dViewH < 1e-6) ? 1 : m_dViewH;

    if (this->height() == 0) {
        aspect = (GLdouble)this->width();
    } else {
        aspect = (GLdouble)this->width() / (GLdouble)this->height();
    }

    m_dXTrans = -(m_rect.left() + m_rect.right()) / 2.0f;
    m_dYTrans = -(m_rect.top() + m_rect.bottom()) / 2.0f;

    if (m_dViewW < m_dViewH * aspect)
        m_dViewW = m_dViewH * aspect;
    else
        m_dViewH = m_dViewW / aspect;

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
    glPointSize(pointSize);

    painter.save();
    painter.setPen(Qt::white);

    if (!fixedPolygon.empty()) {
        if (mode == DrawPolyline1)
            glColor3f(1, 1, 0);
        else
            glColor3f(0, 1, 0);

        Box2D box = calcBoundingBox(fixedPolygon);
        Point pc = box.center();
        QPointF p0(pc.x, pc.y);

        View2Scr(p0);

        painter.drawText(p0, "P1");
        glPointSize(5);
        glBegin(GL_POINTS);
        glVertex2d(fixedPolygon[0].x, fixedPolygon[0].y);
        glEnd();

        glBegin(GL_LINE_STRIP);
        for (const auto& pt : fixedPolygon) {
            glVertex2d(pt.x, pt.y);
        }
        glVertex2d(fixedPolygon[0].x, fixedPolygon[0].y);
        glEnd();
    }

    Polyline movingPolygonForDraw = animationEnabled ? animationBaseMovingPolygon : movingPolygon;
    if (animationEnabled && !movingPolygonForDraw.empty()) {
        movingPolygonForDraw.translate(animationRefPoint - movingPolygonForDraw[0]);
    }

    if (!movingPolygonForDraw.empty()) {

        if (mode == DrawPolyline2)
            glColor3f(1, 1, 0);
        else
            glColor3f(0, 1, 0);

        Box2D box = calcBoundingBox(movingPolygonForDraw);
        Point pc = box.center();
        QPointF p0(pc.x, pc.y);

        View2Scr(p0);
        painter.drawText(p0, "P2");

        glPointSize(5);
        glBegin(GL_POINTS);
        glVertex2d(movingPolygonForDraw[0].x, movingPolygonForDraw[0].y);
        glEnd();
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glBegin(GL_POLYGON);
        for (const auto& pt : movingPolygonForDraw) {
            glVertex2d(pt.x, pt.y);
        }
        glEnd();

        glColor3f(1, 0, 0);
        glPointSize(9);
        glBegin(GL_POINTS);
        glVertex2d(movingPolygonForDraw[0].x, movingPolygonForDraw[0].y);
        glEnd();

        QPointF refLabel(movingPolygonForDraw[0].x, movingPolygonForDraw[0].y);
        View2Scr(refLabel);
        refLabel += QPointF(8, -8);
        painter.setPen(Qt::red);
        painter.drawText(refLabel, QString("Ref (%1, %2)")
                          .arg(movingPolygonForDraw[0].x, 0, 'f', 1)
                          .arg(movingPolygonForDraw[0].y, 0, 'f', 1));
        painter.setPen(Qt::white);
    }

    if (!nfps.empty()) {
        glColor3f(1, 1, 1);
        for (const auto& nfp : nfps) {

            glPointSize(5);
            glBegin(GL_POINTS);
            glVertex2d(nfp[0].x, nfp[0].y);
            glEnd();

            glBegin(GL_LINE_STRIP);
            for (const auto& pt : nfp) {
                glVertex2d(pt.x, pt.y);
            }
            if (!nfp.empty()) {
                glVertex2d(nfp[0].x, nfp[0].y);
            }
            glEnd();
        }
    }

    auto drawNestPolygons = [&](std::vector<Polyline>& nestPolys){
        if (!nestPolys.empty()) {
            int idx = 0;
            for (const auto& nfp : nestPolys) {

                if (idx == 0)
                    glColor3f(1, 1, 0);
                else if (idx == 1)
                    glColor3f(0, 1, 1);
                else
                    glColor3f(0.5f, 0.5f, 1.0f);
                idx++;

                glPointSize(5);
                glBegin(GL_POINTS);
                glVertex2d(nfp[0].x, nfp[0].y);
                glEnd();

                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glBegin(GL_POLYGON);
                for (const auto& pt : nfp) {
                    glVertex2d(pt.x, pt.y);
                }
                glEnd();
            }
        }
    };
    drawNestPolygons(nestPoly);
    drawNestPolygons(nestPoly2);


    if (mode == DrawPolyline2 || mode == DrawPolyline1) {
        if (!pTmp.empty()) {

            if (mode == DrawPolyline2)
                glColor3f(1, 1, 0);
            else
                glColor3f(0, 1, 0);

            glBegin(GL_LINE_STRIP);
            for (const auto& pt : pTmp) {
                glVertex2d(pt.x, pt.y);
            }
            glVertex2d(ptCur.x, ptCur.y);
            glVertex2d(pTmp[0].x, pTmp[0].y);
            glEnd();

            glBegin(GL_LINE_STRIP);
            for (const auto& pt : pTmp) {
                glVertex2d(pt.x, pt.y);
            }

            glVertex2d(pTmp[0].x, pTmp[0].y);
            glEnd();
        }
    }

    painter.restore();
}

void MyCtrlView::setPointSize(int size)
{
    pointSize = size;
}

void MyCtrlView::setNFPs(const std::vector<MyCtrlView::Polyline>& nfp)
{
    nfps = nfp;
    update();
}

void MyCtrlView::setPolyline(const MyCtrlView::Polyline& fixed, const MyCtrlView::Polyline& moving)
{
    fixedPolygon = fixed;
    movingPolygon = moving;
    stopNfpAnimation();
    update();
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
}
void MyCtrlView::setNestPoly2(const std::vector<MyCtrlView::Polyline>& np)
{
    nestPoly2 = np;
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
