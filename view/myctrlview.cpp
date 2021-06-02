#include "myctrlview.h"

#include "shapes/s_point.hpp"
#include "shapes/s_polyline.hpp"
#include "shapes/utiltool.h"

#include <QMouseEvent>
#include <QPainter>

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

        glBegin(GL_LINE_STRIP);
        for (const auto& pt : p1) {
            glVertex2d(pt.x, pt.y);
        }
        glVertex2d(p1[0].x, p1[0].y);
        glEnd();
    }

    if (!p2.empty()) {

        if (mode == DrawPolyline2)
            glColor3f(1, 1, 0);
        else
            glColor3f(0, 1, 0);

        Box2D box = calcBoundingBox(p2);
        Point pc = box.center();
        QPointF p0(pc.x, pc.y);

        View2Scr(p0);
        painter.drawText(p0, "P2");

        glBegin(GL_LINE_STRIP);
        for (const auto& pt : p2) {
            glVertex2d(pt.x, pt.y);
        }
        glVertex2d(p2[0].x, p2[0].y);
        glEnd();
    }

    double x, y;
    if (!nfps.empty()) {
        glColor3f(1, 1, 1);
        glBegin(GL_LINE_STRIP);
        for (const auto& nfp : nfps) {
            for (const auto& pt : nfp) {
                glVertex2d(pt.x, pt.y);
                x = pt.x;
                y = pt.y;
            }
        }
        glEnd();
    }

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
}

void MyCtrlView::setPolyline(const MyCtrlView::Polyline& p1, const MyCtrlView::Polyline& p2)
{
    this->p1 = p1;
    this->p2 = p2;
}

void MyCtrlView::getPolyline(MyCtrlView::Polyline& p1, MyCtrlView::Polyline& p2)
{
    p1 = this->p1;
    p2 = this->p2;
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
