#include "widget.h"

#include <QApplication>
#include <QDebug>
#include <QTime>

#include "shapes/s_point.hpp"
#include "shapes/s_polyline.hpp"

#include "nest/nest2d.h"

#include "nest/nfpplacer.h"

#include "test.h"

#include <unordered_map>
void test();

int main(int argc, char* argv[])
{
    qDebug() << "";
    QApplication a(argc, argv);
    Widget w;

    //    test();
    w.show();
    return a.exec();
}

void test()
{
    return;
    S_Polyline2R poly;

    QTime t;
    t.start();
    for (size_t i = 0; i < 2000000; ++i) {
        poly.add(S_Point2R(12.55, i));
    }
    qDebug() << "Push Poly" << t.elapsed();

    t.start();
    S_Polyline2R pcopy(poly);
    qDebug() << "Copy Poly" << t.elapsed();

    t.start();
    S_Polyline2R pmove(std::move(poly));
    qDebug() << "Move Poly" << t.elapsed();

    qDebug() << pmove[999].x << pmove[999].y << pmove[0].x.numerator() << pmove[0].x.denominator();
    qDebug() << pmove[0].angle();

    S_Rational64 r(-400, 300);
    qDebug() << r.numerator() << r.denominator();
}
