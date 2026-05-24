#include "widget.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QDebug>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <random>

#include "shapes/s_point.hpp"
#include "shapes/s_polyline.hpp"
#include "ui_widget.h"

#include "nest/nfp_placer.h"
#include "nest/nester.h"
#include "shapes/utiltool.h"
#include "view/segmentedtabbar.h"
#include "view/myctrlview.h"
#include "view/nestcanvasview.h"
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

QIcon makePolygonThumbnail(const S_Shape2D::Polyline2D& polygon, const QColor& color, const QSize& size = QSize(72, 48))
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    if (polygon.size() < 3)
        return QIcon(pixmap);

    double left = polygon[0].x;
    double right = polygon[0].x;
    double top = polygon[0].y;
    double bottom = polygon[0].y;
    for (const auto& pt : polygon) {
        left = std::min(left, pt.x);
        right = std::max(right, pt.x);
        top = std::min(top, pt.y);
        bottom = std::max(bottom, pt.y);
    }

    const double width = std::max(1e-6, right - left);
    const double height = std::max(1e-6, bottom - top);
    const double margin = 6.0;
    const double scale = std::min((size.width() - margin * 2.0) / width,
                                  (size.height() - margin * 2.0) / height);
    const double offsetX = (size.width() - width * scale) * 0.5;
    const double offsetY = (size.height() - height * scale) * 0.5;

    auto toThumb = [&](const S_Shape2D::Point2D& pt) {
        return QPointF(offsetX + (pt.x - left) * scale,
                       offsetY + (pt.y - top) * scale);
    };

    QPainterPath path;
    path.moveTo(toThumb(polygon[0]));
    for (size_t i = 1; i < polygon.size(); ++i)
        path.lineTo(toThumb(polygon[i]));
    path.closeSubpath();

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QColor fill = color;
    fill.setAlpha(90);
    painter.setPen(QPen(color.darker(130), 2.0));
    painter.setBrush(fill);
    painter.drawPath(path);
    painter.setPen(QPen(Qt::white, 1.0));
    painter.setBrush(color.darker(120));
    painter.drawEllipse(toThumb(polygon[0]), 3.0, 3.0);
    return QIcon(pixmap);
}

QTableWidgetItem* makePreviewItem(const S_Shape2D::ShapePrototype& prototype)
{
    auto* item = makeReadOnlyItem(prototype.name);
    item->setIcon(makePolygonThumbnail(prototype.contour, prototype.color));
    item->setSizeHint(QSize(150, 56));
    return item;
}

QLabel* makeSectionTitle(const QString& text)
{
    auto* label = new QLabel(text);
    label->setObjectName("sectionTitle");
    return label;
}

QFrame* makePanel(const QString& title)
{
    auto* panel = new QFrame;
    panel->setObjectName("panel");
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(10);
    layout->addWidget(makeSectionTitle(title));
    return panel;
}

QFrame* makeViewportPanel(const QString& title, QWidget* viewport)
{
    auto* panel = new QFrame;
    panel->setObjectName("viewportPanel");

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 8, 10, 10);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName("viewportTitle");
    layout->addWidget(titleLabel);
    layout->addWidget(viewport, 1);
    return panel;
}

QFrame* makeStatusCard(const QString& title, QLabel*& valueLabel)
{
    auto* card = new QFrame;
    card->setObjectName("statusCard");

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(2);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName("statusTitle");
    valueLabel = new QLabel("--");
    valueLabel->setObjectName("statusValue");

    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    return card;
}

QLabel* makeThumbnailLabel(const S_Shape2D::Polyline2D& polygon, const QColor& color, const QSize& size = QSize(82, 58))
{
    auto* label = new QLabel;
    label->setFixedSize(size);
    label->setAlignment(Qt::AlignCenter);
    label->setObjectName("shapeThumb");
    label->setPixmap(makePolygonThumbnail(polygon, color, size).pixmap(size));
    return label;
}

QLabel* makeMetaLabel(const QString& text)
{
    auto* label = new QLabel(text);
    label->setObjectName("cardMeta");
    return label;
}

QLabel* makeBadgeLabel(const QString& text)
{
    auto* label = new QLabel(text);
    label->setObjectName("cardBadge");
    label->setAlignment(Qt::AlignCenter);
    return label;
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
    setupModernInterface();

    auto fixedPolygon = createDefaultFixedPolygon();
    auto movingPolygon = createDefaultMovingPolygon();
    fixedPreviewView->setPolyline(fixedPolygon, movingPolygon);
    movingPreviewView->setPolyline(fixedPolygon, movingPolygon);
    ui->openGLWidget->setPolyline(MyCtrlView::Polyline{}, MyCtrlView::Polyline{});
    ui->openGLWidget->setNFPs({});
    ui->comboNfpMethod->setCurrentIndex(1);

    connect(ui->btnDrawP1, SIGNAL(clicked()), this, SLOT(onDrawP1()));
    connect(ui->btnDrawP2, SIGNAL(clicked()), this, SLOT(onDrawP2()));
    connect(ui->btnExec, SIGNAL(clicked()), this, SLOT(onExec()));
    connect(ui->btnNest, SIGNAL(clicked()), this, SLOT(onNest()));
    connect(ui->btnAddShape, SIGNAL(clicked()), this, SLOT(onAddShape()));
    connect(ui->btnGenerateRandomShapes, SIGNAL(clicked()), this, SLOT(onGenerateRandomShapes()));
    connect(ui->btnRemoveShape, SIGNAL(clicked()), this, SLOT(onRemoveShape()));
    connect(ui->btnClearShapes, SIGNAL(clicked()), this, SLOT(onClearShapes()));
    fixedPreviewView->setPolylinesChangedCallback([this]() { syncPreviewViews(); });
    movingPreviewView->setPolylinesChangedCallback([this]() { syncPreviewViews(); });
    connect(&timer, SIGNAL(timeout()), this, SLOT(onTimer()));

    ui->tblShapes->horizontalHeader()->setStretchLastSection(true);
    ui->tblShapes->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tblShapes->verticalHeader()->setVisible(false);
    ui->tblShapes->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    connect(ui->tblShapes, &QTableWidget::itemChanged, this, [this](QTableWidgetItem*) {
        syncShapeLibraryFromTable();
        updateStatusCards();
    });
    refreshShapeTable();
    updateStatusCards();
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

void Widget::updateNestSummaryTable()
{
    if (!resultCardsLayout)
        return;

    while (QLayoutItem* item = resultCardsLayout->takeAt(0)) {
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    for (const auto& prototype : currentNestScene.prototypes) {
        const int placed = placedPrototypeCounts[prototype.id];
        const int requested = std::max(0, prototype.quantity);
        const int unplaced = std::max(0, requested - placed);

        auto* card = new QFrame(resultCardsContainer);
        card->setObjectName("resultCard");
        auto* layout = new QHBoxLayout(card);
        layout->setContentsMargins(10, 9, 10, 9);
        layout->setSpacing(10);
        layout->addWidget(makeThumbnailLabel(prototype.contour, prototype.color, QSize(58, 42)), 0);

        auto* infoLayout = new QVBoxLayout;
        infoLayout->setContentsMargins(0, 0, 0, 0);
        infoLayout->setSpacing(6);
        auto* title = new QLabel(prototype.name);
        title->setObjectName("cardTitle");
        infoLayout->addWidget(title);

        auto* progress = new QProgressBar(card);
        progress->setObjectName("miniProgress");
        progress->setRange(0, std::max(1, requested));
        progress->setValue(std::min(placed, requested));
        progress->setTextVisible(false);
        infoLayout->addWidget(progress);

        auto* metaRow = new QHBoxLayout;
        metaRow->setContentsMargins(0, 0, 0, 0);
        metaRow->setSpacing(6);
        metaRow->addWidget(makeBadgeLabel(QString::fromWCharArray(L"%1/%2").arg(placed).arg(requested)));
        metaRow->addWidget(makeBadgeLabel(QString::fromWCharArray(L"\u672A\u653E %1").arg(unplaced)));
        metaRow->addWidget(makeMetaLabel(QString::fromWCharArray(L"\u9762\u79EF %1").arg(std::fabs(prototype.contour.area()), 0, 'f', 1)));
        metaRow->addStretch(1);
        infoLayout->addLayout(metaRow);

        layout->addLayout(infoLayout, 1);

        resultCardsLayout->addWidget(card);
    }
    resultCardsLayout->addStretch(1);
}

void Widget::resetNestProgress(int totalPieces, int saIterations)
{
    nestTotalPieces = totalPieces;
    nestPlacedCount = 0;
    nestProcessedPieces = 0;
    nestSAIterations = saIterations;
    currentNestPhase = NestPlacing;
    placedPrototypeCounts.clear();

    if (nestProgressBar) {
        nestProgressBar->setRange(0, 1000);
        nestProgressBar->setValue(0);
        nestProgressBar->setFormat(QString::fromWCharArray(L"\u653E\u7F6E\u4E2D... %p%"));
    }
    setNestPhaseText(QString::fromWCharArray(L"\u6392\u7248\u4E2D... \u653E\u7F6E\u9636\u6BB5"));
    updateNestSummaryTable();
}

void Widget::setNestPhaseText(const QString& text)
{
    if (lblNestPhase)
        lblNestPhase->setText(text);
}

void Widget::setupModernInterface()
{
    setWindowTitle(QString::fromWCharArray(L"NFP \u6392\u6837\u5DE5\u4F5C\u53F0"));
    resize(1280, 780);

    ui->btnDrawP1->setText(QString::fromWCharArray(L"\u7ED8\u5236\u57FA\u51C6\u4EF6"));
    ui->btnDrawP2->setText(QString::fromWCharArray(L"\u7ED8\u5236\u8FD0\u52A8\u4EF6"));
    ui->btnExec->setText(QString::fromWCharArray(L"\u8BA1\u7B97 NFP"));
    ui->btnNest->setText(QString::fromWCharArray(L"\u5F00\u59CB\u6392\u7248"));
    ui->btnAddShape->setText(QString::fromWCharArray(L"\u52A0\u5165\u56FE\u5F62\u5E93"));
    ui->btnRemoveShape->setText(QString::fromWCharArray(L"\u79FB\u9664\u9009\u4E2D"));
    ui->btnClearShapes->setText(QString::fromWCharArray(L"\u6E05\u7A7A\u56FE\u5F62\u5E93"));
    ui->btnGenerateRandomShapes->setText(QString::fromWCharArray(L"\u751F\u6210\u968F\u673A\u56FE\u5F62"));
    ui->lblShapeSource->setText(QString::fromWCharArray(L"\u6765\u6E90"));
    ui->lblShapeCount->setText(QString::fromWCharArray(L"\u6570\u91CF"));
    ui->lblStockW->setText(QString::fromWCharArray(L"\u677F\u6750\u5BBD"));
    ui->lblStockH->setText(QString::fromWCharArray(L"\u677F\u6750\u9AD8"));
    ui->lblNestMethod->setText(QString::fromWCharArray(L"\u6392\u7248\u7B97\u6CD5"));
    ui->lblRotSteps->setText(QString::fromWCharArray(L"\u65CB\u8F6C\u6B65\u6570"));
    ui->lblRandomShapeCount->setText(QString::fromWCharArray(L"\u968F\u673A\u56FE\u5F62\u6570"));
    ui->lblRandomAreaRange->setText(QString::fromWCharArray(L"\u9762\u79EF\u8303\u56F4"));
    ui->lblUtilization->setText(QString::fromWCharArray(L"\u5229\u7528\u7387: --"));
    ui->chkRotation->setText(QString::fromWCharArray(L"\u5141\u8BB8\u65CB\u8F6C"));
    ui->chkBLF->setText(QString::fromWCharArray(L"\u7A7A\u9699\u586B\u5145"));
    ui->chkSA->setText(QString::fromWCharArray(L"SA \u4F18\u5316"));

    ui->comboNfpMethod->setItemText(0, QString::fromWCharArray(L"\u79FB\u52A8\u78B0\u649E\u6CD5"));
    ui->comboNfpMethod->setItemText(1, QString::fromWCharArray(L"\u77E2\u91CF\u6BB5\u6CD5"));
    ui->comboNfpMethod->setItemText(2, QString::fromWCharArray(L"Minkowski \u6CD5"));
    ui->comboShapeSource->setItemText(0, QString::fromWCharArray(L"\u57FA\u51C6\u4EF6"));
    ui->comboShapeSource->setItemText(1, QString::fromWCharArray(L"\u8FD0\u52A8\u4EF6"));
    ui->spinRandomMinArea->setPrefix(QString::fromWCharArray(L"\u6700\u5C0F "));
    ui->spinRandomMaxArea->setPrefix(QString::fromWCharArray(L"\u6700\u5927 "));
    ui->tblShapes->setMaximumHeight(QWIDGETSIZE_MAX);
    ui->openGLWidget->setMinimumSize(640, 420);
    ui->lblUtilization->hide();

    ui->btnNest->setProperty("role", "primary");
    ui->btnExec->setProperty("role", "accent");
    ui->btnGenerateRandomShapes->setProperty("role", "accent");
    ui->btnRemoveShape->setProperty("role", "danger");
    ui->btnClearShapes->setProperty("role", "danger");

    setStyleSheet(
        "QWidget { background: #F5F7FA; color: #1F2937; font-family: 'Microsoft YaHei', 'Segoe UI'; font-size: 13px; }"
        "QLabel { background: transparent; }"
        "QFrame#toolbar, QFrame#statusBar, QFrame#panel { background: #FFFFFF; border: 1px solid #DDE3EA; border-radius: 8px; }"
        "QFrame#statusCardRow { background: transparent; border: none; }"
        "QFrame#toolbar { border-radius: 0; border-left: none; border-right: none; border-top: none; }"
        "QFrame#canvasFrame { background: #FFFFFF; border: 1px solid #DDE3EA; border-radius: 8px; }"
        "QFrame#viewportPanel { background: #FFFFFF; border: 1px solid #DDE3EA; border-radius: 8px; }"
        "QLabel#viewportTitle { color: #0F172A; font-size: 13px; font-weight: 600; background: transparent; }"
        "QLabel#sectionTitle { color: #111827; font-size: 14px; font-weight: 600; background: transparent; }"
        "QPushButton { background: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 6px; padding: 7px 12px; min-height: 22px; }"
        "QPushButton:hover { background: #EFF6FF; border-color: #93C5FD; }"
        "QPushButton[role='primary'] { background: #2563EB; color: white; border-color: #2563EB; font-weight: 600; }"
        "QPushButton[role='primary']:hover { background: #1D4ED8; }"
        "QPushButton[role='accent'] { background: #0F766E; color: white; border-color: #0F766E; }"
        "QPushButton[role='accent']:hover { background: #0D9488; }"
        "QPushButton[role='danger'] { color: #B91C1C; border-color: #FCA5A5; }"
        "QPushButton[role='danger']:hover { background: #FEF2F2; }"
        "QComboBox { background: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 6px; padding: 5px 30px 5px 8px; min-height: 24px; }"
        "QComboBox:hover { border-color: #60A5FA; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 26px; border: none; background: transparent; }"
        "QComboBox::down-arrow { image: url(:/icons/arrow_down.xpm); width: 9px; height: 9px; margin-right: 8px; }"
        "QComboBox QAbstractItemView { background: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 6px; selection-background-color: #DBEAFE; selection-color: #1E3A8A; outline: none; padding: 4px; }"
        "QSpinBox, QDoubleSpinBox { background: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 6px; padding: 5px 24px 5px 8px; min-height: 24px; }"
        "QSpinBox:hover, QDoubleSpinBox:hover { border-color: #60A5FA; }"
        "QSpinBox::up-button, QDoubleSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 20px; border: none; background: transparent; }"
        "QSpinBox::down-button, QDoubleSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 20px; border: none; background: transparent; }"
        "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: url(:/icons/arrow_up.xpm); width: 9px; height: 9px; }"
        "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: url(:/icons/arrow_down.xpm); width: 9px; height: 9px; }"
        "QCheckBox { background: transparent; spacing: 8px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 8px; border: 1px solid #94A3B8; background: #FFFFFF; }"
        "QCheckBox::indicator:hover { border-color: #2563EB; }"
        "QCheckBox::indicator:checked { border: 1px solid #2563EB; background: #2563EB; }"
        "QTableWidget { background: white; border: 1px solid #DDE3EA; border-radius: 8px; gridline-color: transparent; alternate-background-color: #F8FAFC; selection-background-color: #DBEAFE; selection-color: #1E3A8A; show-decoration-selected: 1; }"
        "QHeaderView::section { background: #F1F5F9; color: #334155; border: none; padding: 7px; font-weight: 600; }"
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 4px 0 4px 0; }"
        "QScrollBar::handle:vertical { background: #CBD5E1; border-radius: 4px; min-height: 28px; }"
        "QScrollBar::handle:vertical:hover { background: #94A3B8; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; border: none; background: transparent; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        "QScrollBar:horizontal { background: transparent; height: 8px; margin: 0 4px 0 4px; }"
        "QScrollBar::handle:horizontal { background: #CBD5E1; border-radius: 4px; min-width: 28px; }"
        "QScrollBar::handle:horizontal:hover { background: #94A3B8; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; border: none; background: transparent; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"
        "QProgressBar { background: #E2E8F0; border: none; border-radius: 8px; min-height: 16px; max-height: 16px; text-align: center; color: #0F172A; font-size: 11px; font-weight: 600; }"
        "QProgressBar::chunk { background: #2563EB; border-radius: 8px; }"
        "QFrame#cardListSurface { background: #FFFFFF; border: 1px solid #DDE3EA; border-radius: 8px; }"
        "QFrame#shapeCard { background-color: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 8px; }"
        "QFrame#shapeCard:hover { border-color: #93C5FD; background-color: #F1F5F9; }"
        "QFrame#resultCard { background-color: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 8px; }"
        "QLabel#cardTitle { color: #0F172A; font-size: 13px; font-weight: 700; background: transparent; }"
        "QLabel#cardMeta { color: #64748B; font-size: 12px; background: transparent; }"
        "QLabel#cardBadge { color: #334155; background: #F1F5F9; border: 1px solid #E2E8F0; border-radius: 8px; padding: 2px 7px; font-size: 12px; }"
        "QLabel#shapeThumb { background: #FFFFFF; border: 1px solid #E2E8F0; border-radius: 8px; }"
        "QProgressBar#miniProgress { background: #E2E8F0; border: none; border-radius: 3px; min-height: 6px; max-height: 6px; }"
        "QProgressBar#miniProgress::chunk { background: #0F766E; border-radius: 3px; }"
        "QFrame#statusCard { background: #FFFFFF; border: 1px solid #DDE3EA; border-radius: 8px; }"
        "QLabel#statusTitle { color: #64748B; font-size: 12px; background: transparent; }"
        "QLabel#statusValue { color: #0F172A; font-size: 18px; font-weight: 700; background: transparent; }"
    );

    if (QLayout* oldLayout = layout()) {
        QLayoutItem* item = nullptr;
        while ((item = oldLayout->takeAt(0)) != nullptr)
            delete item;
        delete oldLayout;
    }

    fixedPreviewView = new MyCtrlView(this);
    fixedPreviewView->setDisplayLayer(MyCtrlView::DisplayLayer::FixedOnly);
    fixedPreviewView->setMinimumSize(260, 180);

    movingPreviewView = new MyCtrlView(this);
    movingPreviewView->setDisplayLayer(MyCtrlView::DisplayLayer::MovingOnly);
    movingPreviewView->setMinimumSize(260, 180);

    ui->openGLWidget->setDisplayLayer(MyCtrlView::DisplayLayer::FullScene);

    auto* nfpTab = new QWidget(this);
    auto* nfpLayout = new QVBoxLayout(nfpTab);
    nfpLayout->setContentsMargins(8, 8, 8, 8);
    nfpLayout->setSpacing(8);

    auto* nfpLeftPanel = new QWidget(nfpTab);
    auto* nfpLeftLayout = new QVBoxLayout(nfpLeftPanel);
    nfpLeftLayout->setContentsMargins(0, 0, 0, 0);
    nfpLeftLayout->setSpacing(8);

    auto* nfpActionPanel = makePanel(QString::fromWCharArray(L"NFP \u8BA1\u7B97"));
    auto* nfpActionLayout = qobject_cast<QVBoxLayout*>(nfpActionPanel->layout());
    auto* nfpMethodRow = new QHBoxLayout;
    nfpMethodRow->setSpacing(8);
    nfpMethodRow->addWidget(new QLabel(QString::fromWCharArray(L"\u7B97\u6CD5"), nfpActionPanel));
    nfpMethodRow->addWidget(ui->comboNfpMethod, 1);
    nfpActionLayout->addLayout(nfpMethodRow);
    auto* nfpDrawRow = new QHBoxLayout;
    nfpDrawRow->setSpacing(8);
    nfpDrawRow->addWidget(ui->btnDrawP1);
    nfpDrawRow->addWidget(ui->btnDrawP2);
    nfpActionLayout->addLayout(nfpDrawRow);
    nfpActionLayout->addWidget(ui->btnExec);

    nfpLeftLayout->addWidget(nfpActionPanel, 0);
    nfpLeftLayout->addWidget(makeViewportPanel(QString::fromWCharArray(L"\u57FA\u51C6\u4EF6\u8F6E\u5ED3"), fixedPreviewView), 1);
    nfpLeftLayout->addWidget(makeViewportPanel(QString::fromWCharArray(L"\u8FD0\u52A8\u4EF6\u8F6E\u5ED3"), movingPreviewView), 1);

    auto* nfpMotionPanel = makeViewportPanel(QString::fromWCharArray(L"NFP \u8FD0\u52A8\u9A8C\u8BC1"), ui->openGLWidget);

    auto* nfpContent = new QSplitter(Qt::Horizontal, nfpTab);
    nfpContent->setChildrenCollapsible(false);
    nfpContent->addWidget(nfpLeftPanel);
    nfpContent->addWidget(nfpMotionPanel);
    nfpContent->setStretchFactor(0, 0);
    nfpContent->setStretchFactor(1, 1);
    nfpContent->setSizes({340, 860});

    nfpLayout->addWidget(nfpContent, 1);

    auto* libraryTab = new QWidget(this);
    auto* libraryLayout = new QVBoxLayout(libraryTab);
    libraryLayout->setContentsMargins(0, 0, 0, 0);
    libraryLayout->setSpacing(8);

    nestDrawView = new MyCtrlView(this);
    nestDrawView->setDisplayLayer(MyCtrlView::DisplayLayer::FixedOnly);
    nestDrawView->setMinimumSize(620, 440);
    nestDrawView->setDefaultViewBounds(ui->spinStockW->value(), ui->spinStockH->value());
    nestDrawView->setPolyline(MyCtrlView::Polyline{}, MyCtrlView::Polyline{});

    auto* btnStartLibraryDraw = new QPushButton(QString::fromWCharArray(L"\u7ED8\u5236\u96F6\u4EF6"), this);
    auto* btnAddDrawnShape = new QPushButton(QString::fromWCharArray(L"\u52A0\u5165\u56FE\u5F62\u5E93"), this);
    auto* btnClearDrawnShape = new QPushButton(QString::fromWCharArray(L"\u6E05\u7A7A\u7ED8\u5236"), this);
    btnAddDrawnShape->setProperty("role", "accent");
    btnClearDrawnShape->setProperty("role", "danger");

    connect(btnStartLibraryDraw, &QPushButton::clicked, this, [this]() {
        if (!nestDrawView)
            return;
        if (workbenchCanvasStack)
            workbenchCanvasStack->setCurrentWidget(nestDrawView);
        nestDrawView->setMode(DrawPolyline1);
    });
    connect(btnAddDrawnShape, &QPushButton::clicked, this, [this]() {
        if (!nestDrawView)
            return;

        MyCtrlView::Polyline polygon;
        MyCtrlView::Polyline unused;
        nestDrawView->getPolyline(polygon, unused);
        if (polygon.size() < 3) {
            qDebug() << "Cannot add drawn shape; polygon is invalid";
            return;
        }

        addShapePrototype(QString("Drawn-%1").arg(nextPrototypeId),
                          polygon,
                          ui->spinShapeCount->value());
    });
    connect(btnClearDrawnShape, &QPushButton::clicked, this, [this]() {
        if (!nestDrawView)
            return;
        nestDrawView->setMode(View);
        nestDrawView->setPolyline(MyCtrlView::Polyline{}, MyCtrlView::Polyline{});
        if (workbenchCanvasStack)
            workbenchCanvasStack->setCurrentWidget(nestDrawView);
    });
    connect(ui->spinStockW, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (nestDrawView)
            nestDrawView->setDefaultViewBounds(ui->spinStockW->value(), ui->spinStockH->value());
    });
    connect(ui->spinStockH, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (nestDrawView)
            nestDrawView->setDefaultViewBounds(ui->spinStockW->value(), ui->spinStockH->value());
    });

    auto* addPanel = makePanel(QString::fromWCharArray(L"\u7ED8\u5236\u548C\u5165\u5E93"));
    auto* addPanelLayout = qobject_cast<QVBoxLayout*>(addPanel->layout());
    auto* drawActions = new QHBoxLayout;
    drawActions->setSpacing(8);
    drawActions->addWidget(btnStartLibraryDraw);
    drawActions->addWidget(btnAddDrawnShape);
    drawActions->addWidget(btnClearDrawnShape);
    addPanelLayout->addLayout(drawActions);

    auto* sourceRow = new QHBoxLayout;
    sourceRow->addWidget(ui->lblShapeSource);
    sourceRow->addWidget(ui->comboShapeSource, 1);
    sourceRow->addWidget(ui->lblShapeCount);
    sourceRow->addWidget(ui->spinShapeCount);
    addPanelLayout->addLayout(sourceRow);
    addPanelLayout->addWidget(ui->btnAddShape);

    shapeCardsContainer = new QWidget(libraryTab);
    shapeCardsLayout = new QVBoxLayout(shapeCardsContainer);
    shapeCardsLayout->setContentsMargins(0, 0, 0, 0);
    shapeCardsLayout->setSpacing(8);
    shapeCardsLayout->addStretch(1);

    auto* shapeScrollArea = new QScrollArea(libraryTab);
    shapeScrollArea->setWidgetResizable(true);
    shapeScrollArea->setWidget(shapeCardsContainer);
    shapeScrollArea->setMinimumHeight(240);

    auto* randomPanel = makePanel(QString::fromWCharArray(L"\u968F\u673A\u591A\u8FB9\u5F62"));
    auto* randomPanelLayout = qobject_cast<QVBoxLayout*>(randomPanel->layout());
    auto* randomGrid = new QGridLayout;
    randomGrid->setHorizontalSpacing(8);
    randomGrid->setVerticalSpacing(8);
    randomGrid->addWidget(ui->lblRandomShapeCount, 0, 0);
    randomGrid->addWidget(ui->spinRandomShapeCount, 0, 1);
    randomGrid->addWidget(ui->lblRandomAreaRange, 1, 0);
    randomGrid->addWidget(ui->spinRandomMinArea, 1, 1);
    randomGrid->addWidget(ui->spinRandomMaxArea, 2, 1);
    randomPanelLayout->addLayout(randomGrid);
    randomPanelLayout->addWidget(ui->btnGenerateRandomShapes);

    auto* shapeLibraryPanel = makePanel(QString::fromWCharArray(L"\u56FE\u5F62\u5E93"));
    auto* shapeLibraryPanelLayout = qobject_cast<QVBoxLayout*>(shapeLibraryPanel->layout());
    shapeLibraryPanelLayout->addWidget(shapeScrollArea, 1);
    shapeLibraryPanelLayout->addWidget(ui->btnClearShapes);

    libraryLayout->addWidget(randomPanel);
    libraryLayout->addWidget(addPanel);
    libraryLayout->addWidget(shapeLibraryPanel, 1);
    ui->tblShapes->hide();
    ui->btnRemoveShape->hide();

    auto* stockPanel = makePanel(QString::fromWCharArray(L"\u677F\u6750\u548C\u7B97\u6CD5"));
    auto* stockPanelLayout = qobject_cast<QVBoxLayout*>(stockPanel->layout());
    auto* configGrid = new QGridLayout;
    configGrid->setHorizontalSpacing(8);
    configGrid->setVerticalSpacing(8);
    configGrid->addWidget(ui->lblStockW, 0, 0);
    configGrid->addWidget(ui->spinStockW, 0, 1);
    configGrid->addWidget(ui->lblStockH, 1, 0);
    configGrid->addWidget(ui->spinStockH, 1, 1);
    configGrid->addWidget(ui->lblNestMethod, 2, 0);
    configGrid->addWidget(ui->comboNestMethod, 2, 1);
    configGrid->addWidget(ui->lblRotSteps, 3, 0);
    configGrid->addWidget(ui->spinRotSteps, 3, 1);
    stockPanelLayout->addLayout(configGrid);
    stockPanelLayout->addWidget(ui->chkRotation);
    stockPanelLayout->addWidget(ui->chkBLF);
    stockPanelLayout->addWidget(ui->chkSA);
    stockPanelLayout->addWidget(ui->btnNest);

    nestCanvasView = new NestCanvasView(this);
    nestCanvasView->setMinimumSize(620, 440);

    workbenchCanvasStack = new QStackedWidget(this);
    workbenchCanvasStack->addWidget(nestCanvasView);
    workbenchCanvasStack->addWidget(nestDrawView);
    workbenchCanvasStack->setCurrentWidget(nestCanvasView);

    auto* resultListSurface = new QFrame(this);
    resultListSurface->setObjectName("cardListSurface");
    auto* resultListSurfaceLayout = new QVBoxLayout(resultListSurface);
    resultListSurfaceLayout->setContentsMargins(0, 0, 0, 0);
    resultListSurfaceLayout->setSpacing(0);

    resultCardsContainer = new QWidget(resultListSurface);
    resultCardsLayout = new QVBoxLayout(resultCardsContainer);
    resultCardsLayout->setContentsMargins(8, 8, 8, 8);
    resultCardsLayout->setSpacing(8);
    resultCardsLayout->addStretch(1);
    resultListSurfaceLayout->addWidget(resultCardsContainer);

    auto* resultScrollArea = new QScrollArea(this);
    resultScrollArea->setWidgetResizable(true);
    resultScrollArea->setWidget(resultListSurface);

    auto* resultPanel = makePanel(QString::fromWCharArray(L"\u6392\u7248\u7ED3\u679C\u6458\u8981"));
    auto* resultPanelLayout = qobject_cast<QVBoxLayout*>(resultPanel->layout());
    resultPanelLayout->addWidget(resultScrollArea, 1);

    auto* nestControlPanel = new QWidget(this);
    auto* nestControlLayout = new QVBoxLayout(nestControlPanel);
    nestControlLayout->setContentsMargins(0, 0, 0, 0);
    nestControlLayout->setSpacing(8);
    nestControlLayout->addWidget(stockPanel, 2);
    nestControlLayout->addWidget(resultPanel, 3);

    nestProgressBar = new QProgressBar(this);
    nestProgressBar->setRange(0, 1000);
    nestProgressBar->setValue(0);
    nestProgressBar->setTextVisible(true);
    nestProgressBar->setFormat(QString::fromWCharArray(L"\u7B49\u5F85\u6392\u7248"));

    auto* centerPanel = makeViewportPanel(QString::fromWCharArray(L"\u4E3B\u753B\u5E03"), workbenchCanvasStack);
    auto* centerLayout = qobject_cast<QVBoxLayout*>(centerPanel->layout());
    centerLayout->addWidget(nestProgressBar, 0);

    auto* nestContent = new QSplitter(Qt::Horizontal, this);
    nestContent->setChildrenCollapsible(false);
    nestContent->addWidget(libraryTab);
    nestContent->addWidget(centerPanel);
    nestContent->addWidget(nestControlPanel);
    nestContent->setStretchFactor(0, 0);
    nestContent->setStretchFactor(1, 1);
    nestContent->setStretchFactor(2, 0);
    nestContent->setSizes({320, 780, 340});

    auto* nestWorkbenchTab = new QWidget(this);
    auto* nestWorkbenchLayout = new QVBoxLayout(nestWorkbenchTab);
    nestWorkbenchLayout->setContentsMargins(8, 8, 8, 8);
    nestWorkbenchLayout->setSpacing(8);
    nestWorkbenchLayout->addWidget(nestContent, 1);

    auto* statusBar = new QFrame(this);
    statusBar->setObjectName("statusCardRow");
    auto* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(8, 4, 8, 4);
    statusLayout->setSpacing(10);
    statusLayout->addWidget(makeStatusCard(QString::fromWCharArray(L"\u56FE\u5F62\u79CD\u7C7B"), lblStatusShapes));
    statusLayout->addWidget(makeStatusCard(QString::fromWCharArray(L"\u603B\u96F6\u4EF6"), lblStatusTotal));
    statusLayout->addWidget(makeStatusCard(QString::fromWCharArray(L"\u5DF2\u653E\u7F6E"), lblStatusPlaced));
    statusLayout->addWidget(makeStatusCard(QString::fromWCharArray(L"\u672A\u653E\u7F6E"), lblStatusUnplaced));
    statusLayout->addWidget(makeStatusCard(QString::fromWCharArray(L"\u5229\u7528\u7387"), lblStatusUtilization));
    nestWorkbenchLayout->addWidget(statusBar, 0);

    auto* mainTabBar = new SegmentedTabBar(this);
    mainTabBar->addTab(QString::fromWCharArray(L"NFP \u9A8C\u8BC1"));
    mainTabBar->addTab(QString::fromWCharArray(L"\u6392\u7248\u5DE5\u4F5C\u53F0"));

    auto* mainStack = new QStackedWidget(this);
    mainStack->addWidget(nfpTab);
    mainStack->addWidget(nestWorkbenchTab);
    connect(mainTabBar, &SegmentedTabBar::currentChanged, mainStack, &QStackedWidget::setCurrentIndex);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(mainTabBar, 0);
    mainLayout->addWidget(mainStack, 1);
    setLayout(mainLayout);
}

void Widget::syncPreviewViews()
{
    if (!fixedPreviewView || !movingPreviewView || !ui->openGLWidget || syncingPreviewViews)
        return;

    syncingPreviewViews = true;

    MyCtrlView::Polyline fixedPolygon;
    MyCtrlView::Polyline unusedMoving;
    fixedPreviewView->getPolyline(fixedPolygon, unusedMoving);

    MyCtrlView::Polyline unusedFixed;
    MyCtrlView::Polyline movingPolygon;
    movingPreviewView->getPolyline(unusedFixed, movingPolygon);

    timer.stop();
    ui->openGLWidget->stopNfpAnimation();
    ui->openGLWidget->setMode(View);
    ui->openGLWidget->setNFPs({});
    ui->openGLWidget->setPolyline(MyCtrlView::Polyline{}, MyCtrlView::Polyline{});
    fixedPreviewView->setPolyline(fixedPolygon, movingPolygon);
    movingPreviewView->setPolyline(fixedPolygon, movingPolygon);

    syncingPreviewViews = false;
}

void Widget::onDrawP1()
{
    timer.stop();
    ui->openGLWidget->stopNfpAnimation();
    ui->openGLWidget->setMode(View);
    ui->openGLWidget->setNFPs({});
    ui->openGLWidget->setPolyline(MyCtrlView::Polyline{}, MyCtrlView::Polyline{});
    movingPreviewView->setMode(View);
    fixedPreviewView->setMode(DrawPolyline1);
}

void Widget::onDrawP2()
{
    timer.stop();
    ui->openGLWidget->stopNfpAnimation();
    ui->openGLWidget->setMode(View);
    ui->openGLWidget->setNFPs({});
    ui->openGLWidget->setPolyline(MyCtrlView::Polyline{}, MyCtrlView::Polyline{});
    fixedPreviewView->setMode(View);
    movingPreviewView->setMode(DrawPolyline2);
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
    if (shapeCardsLayout) {
        while (QLayoutItem* item = shapeCardsLayout->takeAt(0)) {
            if (QWidget* widget = item->widget())
                widget->deleteLater();
            delete item;
        }

        for (int row = 0; row < static_cast<int>(shapeLibrary.size()); ++row) {
            const auto& prototype = shapeLibrary[row];
            auto* card = new QFrame(shapeCardsContainer);
            card->setObjectName("shapeCard");
            card->setAttribute(Qt::WA_StyledBackground, true);

            auto* cardLayout = new QHBoxLayout(card);
            cardLayout->setContentsMargins(0, 8, 10, 8);
            cardLayout->setSpacing(10);

            auto* colorStrip = new QFrame(card);
            colorStrip->setFixedWidth(4);
            colorStrip->setStyleSheet(QString("background: rgba(%1, %2, %3, 200); border-top-left-radius:8px; border-bottom-left-radius:8px;")
                                           .arg(prototype.color.red())
                                           .arg(prototype.color.green())
                                           .arg(prototype.color.blue()));
            cardLayout->addWidget(colorStrip);
            cardLayout->addWidget(makeThumbnailLabel(prototype.contour, prototype.color, QSize(88, 62)), 0);

            auto* infoLayout = new QVBoxLayout;
            infoLayout->setContentsMargins(0, 0, 0, 0);
            infoLayout->setSpacing(6);

            auto* title = new QLabel(prototype.name);
            title->setObjectName("cardTitle");
            infoLayout->addWidget(title);

            auto* badgeRow = new QHBoxLayout;
            badgeRow->setContentsMargins(0, 0, 0, 0);
            badgeRow->setSpacing(6);
            badgeRow->addWidget(makeBadgeLabel(QString::fromWCharArray(L"\u9762\u79EF %1").arg(std::fabs(prototype.contour.area()), 0, 'f', 1)));
            badgeRow->addWidget(makeBadgeLabel(QString::fromWCharArray(L"%1 \u9876\u70B9").arg(prototype.contour.size())));
            badgeRow->addStretch(1);
            infoLayout->addLayout(badgeRow);

            auto* controlRow = new QHBoxLayout;
            controlRow->setContentsMargins(0, 0, 0, 0);
            controlRow->setSpacing(8);
            controlRow->addWidget(makeMetaLabel(QString::fromWCharArray(L"\u6570\u91CF")));

            auto* quantitySpin = new QSpinBox(card);
            quantitySpin->setRange(0, 999);
            quantitySpin->setValue(std::max(0, prototype.quantity));
            quantitySpin->setFixedWidth(72);
            connect(quantitySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, row](int value) {
                if (row >= 0 && row < static_cast<int>(shapeLibrary.size())) {
                    shapeLibrary[row].quantity = std::max(0, value);
                    updateStatusCards();
                    updateNestSummaryTable();
                }
            });
            controlRow->addWidget(quantitySpin);

            auto* deleteButton = new QPushButton(QString::fromWCharArray(L"\u5220\u9664"), card);
            deleteButton->setProperty("role", "danger");
            deleteButton->setFixedWidth(54);
            connect(deleteButton, &QPushButton::clicked, this, [this, row]() {
                if (row < 0 || row >= static_cast<int>(shapeLibrary.size()))
                    return;
                shapeLibrary.erase(shapeLibrary.begin() + row);
                refreshShapeTable();
            });
            controlRow->addStretch(1);
            controlRow->addWidget(deleteButton);
            infoLayout->addLayout(controlRow);

            cardLayout->addLayout(infoLayout, 1);
            shapeCardsLayout->addWidget(card);
        }
        shapeCardsLayout->addStretch(1);
    }

    QSignalBlocker blocker(ui->tblShapes);
    ui->tblShapes->setRowCount(static_cast<int>(shapeLibrary.size()));
    ui->tblShapes->setColumnCount(5);
    ui->tblShapes->setHorizontalHeaderLabels({
        QString::fromWCharArray(L"\u9884\u89C8 / \u56FE\u5F62"),
        QString::fromWCharArray(L"\u6570\u91CF"),
        QString::fromWCharArray(L"\u9762\u79EF"),
        QString::fromWCharArray(L"\u9876\u70B9"),
        QString::fromWCharArray(L"\u989C\u8272")
    });

    for (int row = 0; row < static_cast<int>(shapeLibrary.size()); ++row) {
        const auto& prototype = shapeLibrary[row];
        auto* quantityItem = new QTableWidgetItem(QString::number(prototype.quantity));
        quantityItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        ui->tblShapes->setItem(row, 0, makePreviewItem(prototype));
        ui->tblShapes->setItem(row, 1, quantityItem);
        ui->tblShapes->setItem(row, 2, makeReadOnlyItem(QString::number(std::fabs(prototype.contour.area()), 'f', 1)));
        ui->tblShapes->setItem(row, 3, makeReadOnlyItem(QString::number(prototype.contour.size())));
        ui->tblShapes->setItem(row, 4, makeColorItem(prototype.color));
        ui->tblShapes->setRowHeight(row, 58);
    }

    ui->tblShapes->setIconSize(QSize(72, 48));
    updateStatusCards();
    updateNestSummaryTable();
}

void Widget::syncShapeLibraryFromTable()
{
    if (ui->tblShapes->isHidden())
        return;

    for (int row = 0; row < ui->tblShapes->rowCount() && row < static_cast<int>(shapeLibrary.size()); ++row) {
        if (!ui->tblShapes->item(row, 1))
            continue;

        bool ok = false;
        int quantity = ui->tblShapes->item(row, 1)->text().toInt(&ok);
        shapeLibrary[row].quantity = ok ? std::max(0, quantity) : shapeLibrary[row].quantity;
    }
}

void Widget::updateStatusCards(int totalPieces, int placedPieces, double utilization)
{
    if (!lblStatusShapes || !lblStatusTotal || !lblStatusPlaced || !lblStatusUnplaced || !lblStatusUtilization)
        return;

    int total = 0;
    for (const auto& prototype : shapeLibrary)
        total += std::max(0, prototype.quantity);

    if (totalPieces >= 0)
        total = totalPieces;

    const int placed = std::max(0, placedPieces);
    const int unplaced = (placedPieces >= 0) ? std::max(0, total - placed) : total;

    lblStatusShapes->setText(QString::number(shapeLibrary.size()));
    lblStatusTotal->setText(QString::number(total));
    lblStatusPlaced->setText(placedPieces >= 0 ? QString::number(placed) : QStringLiteral("--"));
    lblStatusUnplaced->setText(placedPieces >= 0 ? QString::number(unplaced) : QStringLiteral("--"));
    lblStatusUtilization->setText(utilization >= 0.0 ? QString("%1%").arg(utilization, 0, 'f', 1) : QStringLiteral("--"));
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
    Polyline unused;
    fixedPreviewView->getPolyline(fixedPolygon, unused);
    movingPreviewView->getPolyline(unused, movingPolygon);

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
    MyCtrlView::Polyline unused;
    fixedPreviewView->getPolyline(fixedPolygon, unused);
    movingPreviewView->getPolyline(unused, movingPolygon);

    fixedPreviewView->setMode(View);
    movingPreviewView->setMode(View);

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
    if (workbenchCanvasStack && nestCanvasView)
        workbenchCanvasStack->setCurrentWidget(nestCanvasView);
    if (nestDrawView)
        nestDrawView->setMode(View);

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
    currentNestScene = scene;
    resetNestProgress(static_cast<int>(scene.pieces.size()),
                      nestConfig.enableSA ? nestConfig.saIterations : 0);
    updateStatusCards(static_cast<int>(scene.pieces.size()), 0, 0.0);
    nester->setConfig(nestConfig);
    nester->setScene(scene);

    qRegisterMetaType<PolylineList>("PolylineList");
    qRegisterMetaType<S_Shape2D::Polyline2D>("S_Shape2D::Polyline2D");

    auto stockPoly = nester->getStock();
    nestCanvasView->beginNest(stockPoly, scene);

    connect(nester, &S_Shape2D::Nester::stepCompleted,
            this, [this](const std::vector<MyCtrlView::Polyline>& placed,
                         const MyCtrlView::Polyline& /*stock*/,
                         double utilization, int pieceIndex) {
        if (!placed.empty()) {
            if (pieceIndex >= 0)
                nestCanvasView->addPiece(placed.back(), pieceIndex, utilization);
            else
                nestCanvasView->setPieces(placed, utilization);
            nestPlacedCount = static_cast<int>(placed.size());
            if (pieceIndex >= 0 && pieceIndex < static_cast<int>(currentNestScene.pieces.size()))
                placedPrototypeCounts[currentNestScene.pieces[pieceIndex].prototypeId]++;
            updateNestSummaryTable();
            updateStatusCards(-1, static_cast<int>(placed.size()), utilization);
            if (currentNestPhase == NestPlacing && nestProgressBar) {
                const int visibleProgress = qMax(nestProcessedPieces, nestPlacedCount);
                const int base = (nestTotalPieces > 0) ? (visibleProgress * 600 / nestTotalPieces) : 0;
                nestProgressBar->setValue(qMin(base, 600));
            } else if (currentNestPhase == NestBLF && nestProgressBar) {
                const int base = 600 + (nestPlacedCount * 150 / (nestTotalPieces > 0 ? nestTotalPieces : 1));
                nestProgressBar->setValue(qMin(base, 750));
            }
            setNestPhaseText(QString::fromWCharArray(L"\u5DF2\u653E\u7F6E %1/%2 | \u5229\u7528\u7387: %3%")
                                 .arg(nestPlacedCount)
                                 .arg(nestTotalPieces)
                                 .arg(utilization, 0, 'f', 1));
        }
    }, Qt::QueuedConnection);

    connect(nester, &S_Shape2D::Nester::candidatesReady,
            this, [this](const std::vector<MyCtrlView::Polyline>& candidates,
                         const std::vector<MyCtrlView::Polyline>& nfps,
                         const MyCtrlView::Polyline& currentPiece) {
        nestCanvasView->showCandidates(candidates, nfps, currentPiece);
    }, Qt::QueuedConnection);

    connect(nester, &S_Shape2D::Nester::placementProgress,
            this, [this](int processedPieces, int totalPieces) {
        if (totalPieces > 0)
            nestTotalPieces = totalPieces;
        nestProcessedPieces = qMax(0, processedPieces);
        if (currentNestPhase == NestPlacing && nestTotalPieces > 0 && nestProgressBar) {
            const int base = qMin(nestProcessedPieces * 600 / nestTotalPieces, 600);
            nestProgressBar->setValue(qMax(nestProgressBar->value(), base));
            nestProgressBar->setFormat(QString::fromWCharArray(L"\u653E\u7F6E\u4E2D... %p%"));
        }
        setNestPhaseText(QString::fromWCharArray(L"\u5DF2\u5904\u7406 %1/%2 | \u5DF2\u653E\u7F6E %3")
                             .arg(qMin(nestProcessedPieces, nestTotalPieces))
                             .arg(nestTotalPieces)
                             .arg(nestPlacedCount));
    }, Qt::QueuedConnection);

    connect(nester, &S_Shape2D::Nester::saProgress,
            this, [this](int iteration, int totalIterations) {
        const int saBase = 750;
        const int saRange = 250;
        const int saProgress = (totalIterations > 0) ? (iteration * saRange / totalIterations) : 0;
        if (nestProgressBar) {
            nestProgressBar->setValue(saBase + saProgress);
            nestProgressBar->setFormat(QString::fromWCharArray(L"SA\u4F18\u5316\u4E2D... %p%"));
        }
    }, Qt::QueuedConnection);

    connect(nester, &S_Shape2D::Nester::phaseChanged,
            this, [this](const QString& phase) {
        setNestPhaseText(phase);
        if (phase.contains(QString::fromWCharArray(L"BLF")))
            currentNestPhase = NestBLF;
        else if (phase.contains(QString::fromWCharArray(L"SA")))
            currentNestPhase = NestSA;
        else
            currentNestPhase = NestPlacing;
    }, Qt::QueuedConnection);

    connect(nester, &S_Shape2D::Nester::finished,
            this, [this, worker]() {
        if (worker != nester)
            return;

        double util = worker->getUtilization();
        auto placed = worker->getPlacedPolygons();
        qDebug() << "Placed" << placed.size() << "pieces, utilization:"
                 << QString::number(util, 'f', 1) << "%";
        updateStatusCards(-1, static_cast<int>(placed.size()), util);
        nestCanvasView->showCandidates({}, {}, {});
        if (nestProgressBar) {
            nestProgressBar->setValue(1000);
            nestProgressBar->setFormat(QString::fromWCharArray(L"\u5B8C\u6210"));
        }
        setNestPhaseText(QString::fromWCharArray(L"\u6392\u7248\u5B8C\u6210 | \u4EF6\u6570: %1 | \u5229\u7528\u7387: %2%")
                             .arg(placed.size())
                             .arg(util, 0, 'f', 1));
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
    MyCtrlView::Polyline unused;
    fixedPreviewView->getPolyline(fixedPolygon, unused);
    movingPreviewView->getPolyline(unused, movingPolygon);
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
    if (fixedPreviewView && movingPreviewView) {
        fixedPreviewView->setPolyline(fixedPolygon, movingPolygon);
        movingPreviewView->setPolyline(fixedPolygon, movingPolygon);
        syncPreviewViews();
    }
}
