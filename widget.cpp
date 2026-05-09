#include "widget.h"

#include <QTime>
#include <QFile>

#include "shapes/s_point.hpp"
#include "shapes/s_polyline.hpp"
#include "ui_widget.h"

#include "nest/nfpplacer.h"
#include "shapes/utiltool.h"
#include "test.h"

namespace {

void debugPrintPolyline(const QString& name, const MyCtrlView::Polyline& poly)
{
    qDebug().noquote() << name << "vertex count:" << poly.size();
    for (size_t i = 0; i < poly.size(); ++i) {
        qDebug().noquote() << QString("%1[%2] = (%3, %4)")
                              .arg(name)
                              .arg(i)
                              .arg(poly[i].x, 0, 'f', 6)
                              .arg(poly[i].y, 0, 'f', 6);
    }
}

}

Widget::Widget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    auto p1 = GetTestP1();
//    p1.rotate(1.7);
    auto p2 = GetTestP2();
//    p2.rotate(0.4);
    ui->openGLWidget->setPolyline(p1, p2);

    connect(ui->btnDrawP1, SIGNAL(clicked()), this, SLOT(onDrawP1()));
    connect(ui->btnDrawP2, SIGNAL(clicked()), this, SLOT(onDrawP2()));
    connect(ui->btnExec, SIGNAL(clicked()), this, SLOT(onExec()));
    connect(&timer, SIGNAL(timeout()), this, SLOT(onTimer()));

    //OnLoad();
    //    timer.start(1000);
}

Widget::~Widget()
{
    delete ui;
}

void Widget::onDrawP1()
{
    timer.stop();
    ui->openGLWidget->stopNfpAnimation();
    ui->openGLWidget->setMode(DrawPolyline1);
}

void Widget::onDrawP2()
{
    timer.stop();
    ui->openGLWidget->stopNfpAnimation();
    ui->openGLWidget->setMode(DrawPolyline2);
}

void Widget::onExec()
{
    MyCtrlView::Polyline p1, p2;
    ui->openGLWidget->getPolyline(p1, p2);
    if(MyCtrlView::Polyline::Clockwise == p1.orientation())
        p1.reverse();
    S_Shape2D::cleanPolygon(p1);
    S_Shape2D::cleanPolygon(p2);

    if(MyCtrlView::Polyline::Clockwise == p2.orientation())
        p2.reverse();
    S_Shape2D::NfpPlacer placer(p1, p2);

    debugPrintPolyline("P1", p1);
    debugPrintPolyline("P2", p2);

    QTime t;
    t.start();
    if (ui->comboNfpMethod->currentIndex() == 0) {
        placer.exec();
    } else {
        placer.execVectorSegments();
    }
    qDebug() << ui->comboNfpMethod->currentText() << u8"NFP calculate takes " << t.elapsed() <<"ms";

    ui->openGLWidget->setPolyline(p1, p2);
    ui->openGLWidget->setNFPs(placer.getNFPs());
    ui->openGLWidget->startNfpAnimation();
    timer.start(30);
}



void Widget::onTimer()
{
    ui->openGLWidget->advanceNfpAnimation();
}

void Widget::OnSave()
{
    MyCtrlView::Polyline p1, p2;
    ui->openGLWidget->getPolyline(p1, p2);
    S_Shape2D::cleanPolygon(p1);
    S_Shape2D::cleanPolygon(p2);
    QFile file(qApp->applicationDirPath()+"/SaveShape.txt");
    if(file.open(QIODevice::WriteOnly))
    {
        QDataStream stream(&file);
        stream << QString("Shape_P1") << p1.size();
        for(auto iter =  p1.begin() ;iter!=p1.end(); iter++ )
        {
            stream << (*iter).x<<(*iter).y;
        }
        stream << QString("Shape_P2") << p2.size();
        for(auto iter =  p2.begin() ;iter!=p2.end(); iter++ )
        {
            stream << (*iter).x<<(*iter).y;
        }
        file.flush();
        file.close();
    }
    else
    {
        qDebug()<<u8"文件打开的时候出现错误 " << file.errorString();
    }
}

void Widget::OnLoad(const QString &absoluteFilePath)
{
    MyCtrlView::Polyline p1, p2;
    QFile file(absoluteFilePath.isNull()?qApp->applicationDirPath()+"/SaveShape.txt":absoluteFilePath);
    if(file.open(QIODevice::ReadOnly))
    {
        QDataStream stream(&file);
        QString shapeIndex;
        size_t shapeSize;
        stream >> shapeIndex >> shapeSize;
        MyCtrlView::Point pt;
        for(size_t i =0;i<shapeSize;i++)
        {
            stream >> pt.x >> pt.y;
            p1.insert(pt);
        }
        stream >> shapeIndex >> shapeSize;
        for(size_t i =0;i<shapeSize;i++)
        {
            stream >> pt.x >> pt.y;
            p1.insert(pt);
        }
        file.close();
    }
    else
    {
        file.close();
        qDebug()<<u8"文件打开的时候出现错误 " << file.errorString();
    }
    p2 = p1;
    p2.rotate(1.7);
    ui->openGLWidget->setPolyline(p1, p2);
}



