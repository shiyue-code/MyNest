#include "widget.h"

#include <QAbstractItemView>
#include <QElapsedTimer>
#include <QFile>
#include <QDebug>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QTableWidgetItem>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <random>

#include "shapes/s_point.hpp"
#include "shapes/s_polyline.hpp"
#include "ui_widget.h"

#include "nest/nfp_placer.h"
#include "nest/nester.h"
#include "shapes/utiltool.h"
#include "view/nestwindow.h"
#include "test.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

QTableWidgetItem* makeReadOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

QTableWidgetItem* makeColorItem(const QColor& color)
{
    auto* item = makeReadOnlyItem(color.name(QColor::HexRgb));
    item->setBackground(color);
    item->setForeground(color.lightness() < 128 ? Qt::white : Qt::black);
    return item;
}

double cross(const S_Shape2D::Point2D& a, const S_Shape2D::Point2D& b, const S_Shape2D::Point2D& c)
{
    return (b - a).cross(c - a);
}

bool pointOnSegment(const S_Shape2D::Point2D& pt,
                    const S_Shape2D::Point2D& a,
                    const S_Shape2D::Point2D& b)
{
    if (std::fabs(cross(a, b, pt)) > 1e-7)
        return false;

    return pt.x >= std::min(a.x, b.x) - 1e-7
        && pt.x <= std::max(a.x, b.x) + 1e-7
        && pt.y >= std::min(a.y, b.y) - 1e-7
        && pt.y <= std::max(a.y, b.y) + 1e-7;
}

bool segmentsIntersect(const S_Shape2D::Point2D& a0,
                       const S_Shape2D::Point2D& a1,
                       const S_Shape2D::Point2D& b0,
                       const S_Shape2D::Point2D& b1)
{
    const double c1 = cross(a0, a1, b0);
    const double c2 = cross(a0, a1, b1);
    const double c3 = cross(b0, b1, a0);
    const double c4 = cross(b0, b1, a1);

    if (((c1 > 1e-7 && c2 < -1e-7) || (c1 < -1e-7 && c2 > 1e-7))
        && ((c3 > 1e-7 && c4 < -1e-7) || (c3 < -1e-7 && c4 > 1e-7))) {
        return true;
    }

    return pointOnSegment(b0, a0, a1)
        || pointOnSegment(b1, a0, a1)
        || pointOnSegment(a0, b0, b1)
        || pointOnSegment(a1, b0, b1);
}

bool isSimplePolygon(const S_Shape2D::Polyline2D& poly)
{
    const size_t n = poly.size();
    if (n < 3)
        return false;

    for (size_t i = 0; i < n; ++i) {
        const size_t iNext = (i + 1) % n;
        for (size_t j = i + 1; j < n; ++j) {
            const size_t jNext = (j + 1) % n;
            if (i == j || iNext == j || jNext == i)
                continue;

            if (segmentsIntersect(poly[i], poly[iNext], poly[j], poly[jNext]))
                return false;
        }
    }

    return true;
}

bool hasConcaveVertex(const S_Shape2D::Polyline2D& poly)
{
    const size_t n = poly.size();
    if (n < 4)
        return false;

    const double sign = poly.area() >= 0.0 ? 1.0 : -1.0;
    for (size_t i = 0; i < n; ++i) {
        const auto& prev = poly[(i + n - 1) % n];
        const auto& curr = poly[i];
        const auto& next = poly[(i + 1) % n];
        if (sign * cross(prev, curr, next) < -1e-7)
            return true;
    }
    return false;
}

bool isValidGeneratedPolygon(const S_Shape2D::Polyline2D& poly, double minArea, double maxArea)
{
    const double area = std::fabs(poly.area());
    if (poly.size() < 3 || area < minArea || area > maxArea || !isSimplePolygon(poly))
        return false;

    ClipperLib::Paths simplified;
    ClipperLib::SimplifyPolygon(S_Shape2D::polygon2Path(poly), simplified, ClipperLib::pftNonZero);
    int solidCount = 0;
    for (const auto& path : simplified) {
        if (path.size() >= 3 && std::fabs(ClipperLib::Area(path)) > 1.0)
            ++solidCount;
    }

    return solidCount == 1;
}

S_Shape2D::Polyline2D makeRandomPolygon(std::mt19937& rng,
                                        bool preferConcave,
                                        double minArea,
                                        double maxArea)
{
    std::uniform_int_distribution<int> vertexCountDist(5, 12);
    std::uniform_real_distribution<double> radiusDist(22.0, 65.0);
    std::uniform_real_distribution<double> angleJitterDist(-0.22, 0.22);
    std::uniform_real_distribution<double> dentDist(0.32, 0.58);
    std::uniform_real_distribution<double> offsetDist(-45.0, 45.0);
    std::bernoulli_distribution dentChance(preferConcave ? 0.45 : 0.0);

    for (int attempt = 0; attempt < 80; ++attempt) {
        const int vertexCount = vertexCountDist(rng);
        const double step = 2.0 * M_PI / vertexCount;
        const double startAngle = std::uniform_real_distribution<double>(0.0, step)(rng);

        S_Shape2D::Polyline2D polygon;
        bool dented = false;
        for (int i = 0; i < vertexCount; ++i) {
            double radius = radiusDist(rng);
            if (dentChance(rng)) {
                radius *= dentDist(rng);
                dented = true;
            }

            const double angle = startAngle + step * i + angleJitterDist(rng);
            polygon.add({
                std::cos(angle) * radius + offsetDist(rng) * 0.05,
                std::sin(angle) * radius + offsetDist(rng) * 0.05
            });
        }

        S_Shape2D::cleanPolygon(polygon);
        if (polygon.orientation() == S_Shape2D::Polyline2D::Clockwise)
            polygon.reverse();

        if (!preferConcave || dented || hasConcaveVertex(polygon)) {
            if (isValidGeneratedPolygon(polygon, minArea, maxArea))
                return polygon;
        }
    }

    return {};
}

}

Widget::Widget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    auto fixedPolygon = createDefaultFixedPolygon();
    auto movingPolygon = createDefaultMovingPolygon();
    ui->openGLWidget->setPolyline(fixedPolygon, movingPolygon);
    ui->comboNfpMethod->setCurrentIndex(1);

    connect(ui->btnDrawP1, SIGNAL(clicked()), this, SLOT(onDrawP1()));
    connect(ui->btnDrawP2, SIGNAL(clicked()), this, SLOT(onDrawP2()));
    connect(ui->btnExec, SIGNAL(clicked()), this, SLOT(onExec()));
    connect(ui->btnNest, SIGNAL(clicked()), this, SLOT(onNest()));
    connect(ui->btnAddShape, SIGNAL(clicked()), this, SLOT(onAddShape()));
    connect(ui->btnGenerateRandomShapes, SIGNAL(clicked()), this, SLOT(onGenerateRandomShapes()));
    connect(ui->btnRemoveShape, SIGNAL(clicked()), this, SLOT(onRemoveShape()));
    connect(ui->btnClearShapes, SIGNAL(clicked()), this, SLOT(onClearShapes()));
    connect(&timer, SIGNAL(timeout()), this, SLOT(onTimer()));

    ui->tblShapes->horizontalHeader()->setStretchLastSection(true);
    ui->tblShapes->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tblShapes->verticalHeader()->setVisible(false);
    ui->tblShapes->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    refreshShapeTable();
}

Widget::~Widget()
{
    timer.stop();
    if (nestThread) {
        nestThread->quit();
        nestThread->wait();
    }
    delete nester;
    nester = nullptr;
    delete nestThread;
    nestThread = nullptr;
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

QColor Widget::colorForShape(int index) const
{
    static const QColor colors[] = {
        QColor("#4C78A8"),
        QColor("#F58518"),
        QColor("#54A24B"),
        QColor("#E45756"),
        QColor("#72B7B2"),
        QColor("#B279A2"),
        QColor("#FF9DA6"),
        QColor("#9D755D")
    };
    return colors[index % (sizeof(colors) / sizeof(colors[0]))];
}

void Widget::addShapePrototype(const QString& name, const Polyline& polygon, int quantity)
{
    Polyline contour = polygon;
    S_Shape2D::cleanPolygon(contour);
    if (contour.size() < 3) {
        qDebug() << "Cannot add invalid shape" << name;
        return;
    }

    if (contour.orientation() == Polyline::Clockwise)
        contour.reverse();

    S_Shape2D::ShapePrototype prototype;
    prototype.id = nextPrototypeId++;
    prototype.name = name;
    prototype.contour = contour;
    prototype.color = colorForShape(static_cast<int>(shapeLibrary.size()));
    prototype.quantity = std::max(1, quantity);
    prototype.enabled = true;

    shapeLibrary.push_back(prototype);
    refreshShapeTable();
}

void Widget::refreshShapeTable()
{
    QSignalBlocker blocker(ui->tblShapes);
    ui->tblShapes->setRowCount(static_cast<int>(shapeLibrary.size()));
    ui->tblShapes->setColumnCount(5);
    ui->tblShapes->setHorizontalHeaderLabels({
        QString::fromWCharArray(L"\u56FE\u5F62"),
        QString::fromWCharArray(L"\u6570\u91CF"),
        QString::fromWCharArray(L"\u9762\u79EF"),
        QString::fromWCharArray(L"\u9876\u70B9"),
        QString::fromWCharArray(L"\u989C\u8272")
    });

    for (int row = 0; row < static_cast<int>(shapeLibrary.size()); ++row) {
        const auto& prototype = shapeLibrary[row];
        auto* quantityItem = new QTableWidgetItem(QString::number(prototype.quantity));
        quantityItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        ui->tblShapes->setItem(row, 0, makeReadOnlyItem(prototype.name));
        ui->tblShapes->setItem(row, 1, quantityItem);
        ui->tblShapes->setItem(row, 2, makeReadOnlyItem(QString::number(std::fabs(prototype.contour.area()), 'f', 1)));
        ui->tblShapes->setItem(row, 3, makeReadOnlyItem(QString::number(prototype.contour.size())));
        ui->tblShapes->setItem(row, 4, makeColorItem(prototype.color));
    }
}

void Widget::syncShapeLibraryFromTable()
{
    for (int row = 0; row < ui->tblShapes->rowCount() && row < static_cast<int>(shapeLibrary.size()); ++row) {
        if (!ui->tblShapes->item(row, 1))
            continue;

        bool ok = false;
        int quantity = ui->tblShapes->item(row, 1)->text().toInt(&ok);
        shapeLibrary[row].quantity = ok ? std::max(0, quantity) : shapeLibrary[row].quantity;
    }
}

S_Shape2D::NestScene Widget::buildNestSceneFromUi()
{
    syncShapeLibraryFromTable();

    S_Shape2D::NestScene scene;
    scene.stockWidth = ui->spinStockW->value();
    scene.stockHeight = ui->spinStockH->value();

    if (shapeLibrary.empty())
        qDebug() << "Please add shapes to the shape library before nesting";

    scene.prototypes = shapeLibrary;

    scene.pieces = S_Shape2D::expandScenePieces(scene);
    return scene;
}

Widget::Polyline Widget::selectedSourcePolygon(QString* sourceName)
{
    Polyline fixedPolygon;
    Polyline movingPolygon;
    ui->openGLWidget->getPolyline(fixedPolygon, movingPolygon);

    if (ui->comboShapeSource->currentIndex() == 0) {
        if (sourceName)
            *sourceName = ui->comboShapeSource->currentText();
        return fixedPolygon;
    }

    if (sourceName)
        *sourceName = ui->comboShapeSource->currentText();
    return movingPolygon;
}

void Widget::onAddShape()
{
    QString sourceName;
    Polyline polygon = selectedSourcePolygon(&sourceName);
    addShapePrototype(QString("%1-%2").arg(sourceName).arg(nextPrototypeId),
                      polygon,
                      ui->spinShapeCount->value());
}

void Widget::onGenerateRandomShapes()
{
    static std::mt19937 rng(std::random_device{}());

    const int count = ui->spinRandomShapeCount->value();
    const double minArea = std::min(ui->spinRandomMinArea->value(), ui->spinRandomMaxArea->value());
    const double maxArea = std::max(ui->spinRandomMinArea->value(), ui->spinRandomMaxArea->value());
    int generated = 0;
    for (int i = 0; i < count; ++i) {
        const bool preferConcave = (i % 2) == 1;
        Polyline polygon = makeRandomPolygon(rng, preferConcave, minArea, maxArea);
        if (polygon.size() < 3) {
            qDebug() << "Random polygon generation skipped; area range may be too restrictive"
                     << minArea << maxArea;
            continue;
        }
        addShapePrototype(QString("Random-%1").arg(nextPrototypeId), polygon, 1);
        ++generated;
    }

    qDebug() << "Generated" << generated << "/" << count
             << "random legal polygons with area range" << minArea << maxArea;
}

void Widget::onRemoveShape()
{
    int row = ui->tblShapes->currentRow();
    if (row < 0 || row >= static_cast<int>(shapeLibrary.size()))
        return;

    shapeLibrary.erase(shapeLibrary.begin() + row);
    refreshShapeTable();
}

void Widget::onClearShapes()
{
    shapeLibrary.clear();
    refreshShapeTable();
}

void Widget::onExec()
{
    MyCtrlView::Polyline fixedPolygon;
    MyCtrlView::Polyline movingPolygon;
    ui->openGLWidget->getPolyline(fixedPolygon, movingPolygon);
    if(MyCtrlView::Polyline::Clockwise == fixedPolygon.orientation())
        fixedPolygon.reverse();
    S_Shape2D::cleanPolygon(fixedPolygon);
    S_Shape2D::cleanPolygon(movingPolygon);

    if(MyCtrlView::Polyline::Clockwise == movingPolygon.orientation())
        movingPolygon.reverse();
    S_Shape2D::NfpPlacer placer(fixedPolygon, movingPolygon);

    debugPrintPolyline("P1", fixedPolygon);
    debugPrintPolyline("P2", movingPolygon);

    QElapsedTimer t;
    t.start();
    if (ui->comboNfpMethod->currentIndex() == 0) {
        placer.exec();
    } else if (ui->comboNfpMethod->currentIndex() == 1) {
        placer.execVectorSegments();
    } else {
        placer.execMinkowski();
    }
    qDebug() << ui->comboNfpMethod->currentText() << u8"NFP calculate takes " << t.elapsed() <<"ms";

    ui->openGLWidget->setPolyline(fixedPolygon, movingPolygon);
    ui->openGLWidget->setNFPs(placer.getNFPs());
    ui->openGLWidget->startNfpAnimation();
    timer.start(30);
}

void Widget::onNest()
{
    S_Shape2D::NestScene scene = buildNestSceneFromUi();

    if (scene.pieces.empty()) {
        qDebug() << "No pieces to nest";
        return;
    }

    qDebug() << "Nesting scene prototypes:" << scene.prototypes.size()
             << "pieces:" << scene.pieces.size();

    if (nestThread) {
        nestThread->quit();
        nestThread->wait();
    }
    delete nester;
    nester = nullptr;
    delete nestThread;
    nestThread = nullptr;

    nester = new S_Shape2D::Nester;
    auto* worker = nester;
    nester->setStock(ui->spinStockW->value(), ui->spinStockH->value());

    S_Shape2D::Nester::Config nestConfig;
    nestConfig.stockWidth = ui->spinStockW->value();
    nestConfig.stockHeight = ui->spinStockH->value();
    nestConfig.nfpMethod = ui->comboNfpMethod->currentIndex();
    nestConfig.allowRotation = ui->chkRotation->isChecked();
    nestConfig.rotationSteps = ui->spinRotSteps->value();
    nestConfig.enableBLF = ui->chkBLF->isChecked();
    nestConfig.enableSA = ui->chkSA->isChecked();
    nester->setConfig(nestConfig);
    nester->setScene(scene);

    qRegisterMetaType<PolylineList>("PolylineList");
    qRegisterMetaType<S_Shape2D::Polyline2D>("S_Shape2D::Polyline2D");

    if (!nestWindow)
        nestWindow = new NestWindow(this);

    auto stockPoly = nester->getStock();
    nestWindow->beginNest(stockPoly, scene, static_cast<int>(scene.pieces.size()),
                          nestConfig.enableSA ? nestConfig.saIterations : 0);

    connect(nester, &S_Shape2D::Nester::stepCompleted,
            this, [this](const std::vector<MyCtrlView::Polyline>& placed,
                         const MyCtrlView::Polyline& /*stock*/,
                         double utilization, int pieceIndex) {
        if (!placed.empty()) {
            if (pieceIndex >= 0)
                nestWindow->addPlacedPiece(placed.back(), utilization, pieceIndex);
            else
                nestWindow->setPlacedPieces(placed, utilization);
        }
    }, Qt::QueuedConnection);

    connect(nester, &S_Shape2D::Nester::candidatesReady,
            this, [this](const std::vector<MyCtrlView::Polyline>& candidates,
                         const std::vector<MyCtrlView::Polyline>& nfps,
                         const MyCtrlView::Polyline& currentPiece) {
        nestWindow->showCandidates(candidates, nfps, currentPiece);
    }, Qt::QueuedConnection);

    connect(nester, &S_Shape2D::Nester::placementProgress,
            this, [this](int processedPieces, int totalPieces) {
        nestWindow->setPlacementProgress(processedPieces, totalPieces);
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
            this, [this, worker]() {
        if (worker != nester)
            return;

        double util = worker->getUtilization();
        auto placed = worker->getPlacedPolygons();
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

    connect(nestThread, &QThread::started, nester, [worker, useBL]() {
        QElapsedTimer t;
        t.start();
        if (useBL)
            worker->execBL();
        else
            worker->execGreedy();
        qDebug() << "Nesting took" << t.elapsed() << "ms";
        emit worker->finished();
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
    MyCtrlView::Polyline fixedPolygon;
    MyCtrlView::Polyline movingPolygon;
    ui->openGLWidget->getPolyline(fixedPolygon, movingPolygon);
    S_Shape2D::cleanPolygon(fixedPolygon);
    S_Shape2D::cleanPolygon(movingPolygon);
    QFile file(qApp->applicationDirPath()+"/SaveShape.txt");
    if(file.open(QIODevice::WriteOnly))
    {
        QDataStream stream(&file);
        stream << QString("Shape_P1") << fixedPolygon.size();
        for(auto iter =  fixedPolygon.begin() ;iter!=fixedPolygon.end(); iter++ )
        {
            stream << (*iter).x<<(*iter).y;
        }
        stream << QString("Shape_P2") << movingPolygon.size();
        for(auto iter =  movingPolygon.begin() ;iter!=movingPolygon.end(); iter++ )
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

void Widget::OnLoad(const QString& absoluteFilePath)
{
    MyCtrlView::Polyline fixedPolygon;
    MyCtrlView::Polyline movingPolygon;
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
            fixedPolygon.insert(pt);
        }
        stream >> shapeIndex >> shapeSize;
        for(size_t i =0;i<shapeSize;i++)
        {
            stream >> pt.x >> pt.y;
            movingPolygon.insert(pt);
        }
        file.close();
    }
    else
    {
        file.close();
        qDebug()<<u8"文件打开的时候出现错误 " << file.errorString();
    }
    ui->openGLWidget->setPolyline(fixedPolygon, movingPolygon);
}
