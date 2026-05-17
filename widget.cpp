#include "widget.h"

#include <QTime>
#include <QFile>
#include <QThread>

#include "shapes/s_point.hpp"
#include "shapes/s_polyline.hpp"
#include "ui_widget.h"

#include "nest/nfpplacer.h"
#include "nest/nester.h"
#include "shapes/utiltool.h"
#include "view/nestwindow.h"
#include "test.h"

typedef std::vector<S_Shape2D::Polyline2D> PolylineList;
Q_DECLARE_METATYPE(PolylineList)
Q_DECLARE_METATYPE(S_Shape2D::Polyline2D)

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
    auto p2 = GetTestP2();
    ui->openGLWidget->setPolyline(p1, p2);

    connect(ui->btnDrawP1, SIGNAL(clicked()), this, SLOT(onDrawP1()));
    connect(ui->btnDrawP2, SIGNAL(clicked()), this, SLOT(onDrawP2()));
    connect(ui->btnExec, SIGNAL(clicked()), this, SLOT(onExec()));
    connect(ui->btnNest, SIGNAL(clicked()), this, SLOT(onNest()));
    connect(&timer, SIGNAL(timeout()), this, SLOT(onTimer()));
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
    } else if (ui->comboNfpMethod->currentIndex() == 1) {
        placer.execVectorSegments();
    } else {
        placer.execMinkowski();
    }
    qDebug() << ui->comboNfpMethod->currentText() << u8"NFP calculate takes " << t.elapsed() <<"ms";

    ui->openGLWidget->setPolyline(p1, p2);
    ui->openGLWidget->setNFPs(placer.getNFPs());
    ui->openGLWidget->startNfpAnimation();
    timer.start(30);
}

void Widget::onNest()
{
    MyCtrlView::Polyline p1, p2;
    ui->openGLWidget->getPolyline(p1, p2);
    S_Shape2D::cleanPolygon(p1);
    S_Shape2D::cleanPolygon(p2);

    int p1Count = ui->spinP1Count->value();
    int p2Count = ui->spinP2Count->value();

    if ((p1Count > 0 && p1.size() < 3) || (p2Count > 0 && p2.size() < 3)) {
        qDebug() << "Need valid P1 and P2 polygons for nesting";
        return;
    }

    if (p1Count == 0 && p2Count == 0) {
        qDebug() << "No pieces to nest";
        return;
    }

    std::vector<MyCtrlView::Polyline> pieces;
    for (int i = 0; i < p1Count; ++i)
        pieces.push_back(p1);
    for (int i = 0; i < p2Count; ++i)
        pieces.push_back(p2);

    qDebug() << "Nesting" << p1Count << "x P1 +" << p2Count << "x P2 =" << pieces.size() << "pieces";

    if (nestThread) {
        nestThread->quit();
        nestThread->wait();
        delete nestThread;
    }
    delete nester;

    nester = new S_Shape2D::Nester;
    nester->setStock(ui->spinStockW->value(), ui->spinStockH->value());
    nester->setPolygons(pieces);

    S_Shape2D::Nester::Config cfg;
    cfg.stockWidth = ui->spinStockW->value();
    cfg.stockHeight = ui->spinStockH->value();
    cfg.nfpMethod = ui->comboNfpMethod->currentIndex();
    cfg.allowRotation = ui->chkRotation->isChecked();
    cfg.rotationSteps = ui->spinRotSteps->value();
    cfg.enableBacktrack = ui->chkBacktrack->isChecked();
    cfg.enableBLF = ui->chkBLF->isChecked();
    cfg.enableSA = ui->chkSA->isChecked();
    nester->setConfig(cfg);

    qRegisterMetaType<PolylineList>("PolylineList");
    qRegisterMetaType<S_Shape2D::Polyline2D>("S_Shape2D::Polyline2D");

    if (!nestWindow)
        nestWindow = new NestWindow(this);

    auto stockPoly = nester->getStock();
    nestWindow->beginNest(stockPoly, pieces.size(), cfg.enableSA ? cfg.saIterations : 0);

    connect(nester, &S_Shape2D::Nester::stepCompleted,
            this, [this](const std::vector<MyCtrlView::Polyline>& placed,
                         const MyCtrlView::Polyline& /*stock*/,
                         double utilization, int /*pieceIndex*/) {
        if (!placed.empty()) {
            nestWindow->addPlacedPiece(placed.back(), utilization);
        }
    }, Qt::QueuedConnection);

    connect(nester, &S_Shape2D::Nester::candidatesReady,
            this, [this](const std::vector<MyCtrlView::Polyline>& candidates,
                         const std::vector<MyCtrlView::Polyline>& nfps,
                         const MyCtrlView::Polyline& currentPiece) {
        nestWindow->showCandidates(candidates, nfps, currentPiece);
    }, Qt::QueuedConnection);

    connect(nester, &S_Shape2D::Nester::saProgress,
            this, [this](int iteration, int totalIterations) {
        nestWindow->setSAProgress(iteration, totalIterations);
    }, Qt::QueuedConnection);

    connect(nester, &S_Shape2D::Nester::phaseChanged,
            this, [this](const QString& phase) {
        nestWindow->setPhase(phase);
    }, Qt::QueuedConnection);

    connect(nester, &S_Shape2D::Nester::finished,
            this, [this]() {
        double util = nester->getUtilization();
        auto placed = nester->getPlacedPolygons();
        qDebug() << "Placed" << placed.size() << "pieces, utilization:"
                 << QString::number(util, 'f', 1) << "%";
        ui->lblUtilization->setText(QString("利用率: %1%").arg(util, 0, 'f', 1));
        nestWindow->endNest();
        nestWindow->show();
        nestWindow->raise();
        nestWindow->activateWindow();
    }, Qt::QueuedConnection);

    bool useBL = (ui->comboNestMethod->currentIndex() == 0);

    nestThread = new QThread;
    nester->moveToThread(nestThread);

    connect(nestThread, &QThread::started, nester, [this, useBL]() {
        QTime t;
        t.start();
        if (useBL)
            nester->execBL();
        else
            nester->execGreedy();
        qDebug() << "Nesting took" << t.elapsed() << "ms";
        emit nester->finished();
    });

    connect(nester, &S_Shape2D::Nester::finished, nestThread, &QThread::quit);

    nestThread->start();
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
