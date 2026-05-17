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

    //    Box2D box1 = calcBoundingBox(p1);
    //    Box2D box2 = calcBoundingBox(p2);
    //    box = box1 | box2;
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
    m_dViewW = m_rect.width() * 0.51 /** (sqrt(5.0))/3.0*/;
    m_dViewW = (m_dViewW < 1e-6) ? 1 : m_dViewW;

    m_dViewH = m_rect.height() * 0.51 /* * (sqrt(5.0))/3.0*/;
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

    if (!p1.empty()) {
        if (mode == DrawPolyline1)
            glColor3f(1, 1, 0);
        else
            glColor3f(0, 1, 0);

        Box2D box = calcBoundingBox(p1);
        Point pc = box.center();
        QPointF p0(pc.x, pc.y);

        View2Scr(p0);

        painter.drawText(p0, "P1");
        glPointSize(5);
        glBegin(GL_POINTS);
        glVertex2d(p1[0].x, p1[0].y);
        glEnd();

        glBegin(GL_LINE_STRIP);
        for (const auto& pt : p1) {
            glVertex2d(pt.x, pt.y);
        }
        glVertex2d(p1[0].x, p1[0].y);
        glEnd();
    }

    Polyline p2Draw = animationEnabled ? animationBaseP2 : p2;
    if (animationEnabled && !p2Draw.empty()) {
        p2Draw.translate(animationRefPoint - p2Draw[0]);
    }

    if (!p2Draw.empty()) {

        if (mode == DrawPolyline2)
            glColor3f(1, 1, 0);
        else
            glColor3f(0, 1, 0);

        Box2D box = calcBoundingBox(p2Draw);
        Point pc = box.center();
        QPointF p0(pc.x, pc.y);

        View2Scr(p0);
        painter.drawText(p0, "P2");

        glPointSize(5);
        glBegin(GL_POINTS);
        glVertex2d(p2Draw[0].x, p2Draw[0].y);
        glEnd();
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glBegin(GL_POLYGON);
        for (const auto& pt : p2Draw) {
            glVertex2d(pt.x, pt.y);
        }
        glEnd();

        glColor3f(1, 0, 0);
        glPointSize(9);
        glBegin(GL_POINTS);
        glVertex2d(p2Draw[0].x, p2Draw[0].y);
        glEnd();

        QPointF refLabel(p2Draw[0].x, p2Draw[0].y);
        View2Scr(refLabel);
        refLabel += QPointF(8, -8);
        painter.setPen(Qt::red);
        painter.drawText(refLabel, QString("Ref (%1, %2)")
                         .arg(p2Draw[0].x, 0, 'f', 1)
                         .arg(p2Draw[0].y, 0, 'f', 1));
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

    //        图形跟随鼠标
    //    if (!p1.empty()) {
    //        auto p = p2;
    //        //        p.rotate(pi);
    //        double xs = ptCur.x - p[0].x;
    //        double ys = ptCur.y - p[0].y;
    //        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    //        glBegin(GL_POLYGON);
    //        for (const auto& pt : p) {
    //            glVertex2d(pt.x + xs, pt.y + ys);
    //        }
    //        glEnd();
    //        glColor3f(1, 1, 1);
    //    }

    auto func = [&](std::vector<Polyline>& nestPolys){
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
    func(nestPoly);
    func(nestPoly2);


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

void MyCtrlView::setPolyline(const MyCtrlView::Polyline& p1, const MyCtrlView::Polyline& p2)
{
    this->p1 = p1;
    this->p2 = p2;
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

    if (animationPath.size() < 4 || p2.empty()) {
        stopNfpAnimation();
        return;
    }

    animationBaseP2 = p2;
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

void MyCtrlView::getPolyline(MyCtrlView::Polyline& p1, MyCtrlView::Polyline& p2)
{
    p1 = this->p1;
    p2 = this->p2;
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
                p1 = pTmp;
            } break;
            case DrawPolyline2: {
                p2 = pTmp;
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
