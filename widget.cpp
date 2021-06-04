#include "widget.h"
#include "shapes/s_point.hpp"
#include "shapes/s_polyline.hpp"
#include "ui_widget.h"

#include <QTime>

#include "nest/nest2d.h"
#include "nest/nfpplacer.h"
#include "shapes/utiltool.h"
#include "test.h"

Widget::Widget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    auto p2 = GetTestP1();
    p2.rotate(S_Shape2D::pi);
    ui->openGLWidget->setPolyline(GetTestP1(), p2);

    connect(ui->btnDrawP1, SIGNAL(clicked()), this, SLOT(onDrawP1()));
    connect(ui->btnDrawP2, SIGNAL(clicked()), this, SLOT(onDrawP2()));
    connect(ui->btnExec, SIGNAL(clicked()), this, SLOT(onExec()));
    connect(ui->btnNest, SIGNAL(clicked()), this, SLOT(onNest()));
}

Widget::~Widget()
{
    delete ui;
}

void Widget::onDrawP1()
{
    ui->openGLWidget->setMode(DrawPolyline1);
}

void Widget::onDrawP2()
{
    ui->openGLWidget->setMode(DrawPolyline2);
}

void Widget::onExec()
{
    MyCtrlView::Polyline p1, p2;
    ui->openGLWidget->getPolyline(p1, p2);

    S_Shape2D::cleanPolygon(p1);
    S_Shape2D::cleanPolygon(p2);

    S_Shape2D::NfpPlacer placer(p1, p2);

    QTime t;
    t.start();
    placer.exec();
    qDebug() << "NFP计算耗时" << t.elapsed();

    ui->openGLWidget->setNFPs(placer.getNFPs());
}

void Widget::onNest()
{
    MyCtrlView::Polyline p1, p2;
    ui->openGLWidget->getPolyline(p1, p2);

    S_Shape2D::cleanPolygon(p1);
    S_Shape2D::cleanPolygon(p2);
    S_Shape2D::NestSingle2D::Box box({ 0, 0 }, { 1000, 1000 });

    S_Shape2D::NestSingle2D nester(box, p1);
    nester.exec();
    ui->openGLWidget->setNestPoly(nester.result());
}
