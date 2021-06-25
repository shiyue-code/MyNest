#ifndef MYCTRLVIEW_H
#define MYCTRLVIEW_H

#include "kwctrlview.h"
#include "nest/nfpplacer.h"

#include <vector>

enum {
    View,
    DrawPolyline1,
    DrawPolyline2,
};

class MyCtrlView : public KWCtrlView {

public:
    using Polyline = S_Polyline2D;
    using Point = Polyline::Point;

public:
    MyCtrlView(QWidget* parent = nullptr);

    void ResetView() override;

    void setMode(int mode);
    void setPointSize(int size);
    void setNFPs(const std::vector<Polyline>& nfp);
    void setPolyline(const Polyline& p1, const Polyline& p2);

    void getPolyline(Polyline& p1, Polyline& p2);

    void setNestPoly(const std::vector<Polyline>& np);
    void setNestPoly2(const std::vector<MyCtrlView::Polyline> &np);

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* evt) override;

protected:
    void DrawGLSence(QPainter& painter) override;

private:
    int pointSize = 5;

    Polyline p1;
    Polyline p2;
    std::vector<Polyline> nfps, nestPoly,nestPoly2;

    Polyline pTmp;
    Point ptCur;

    int mode = View;
};

#endif // MYCTRLVIEW_H
