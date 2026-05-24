#include "nestwindow.h"
#include "kwctrlview.h"
#include "shapes/s_box.hpp"
#include "shapes/utiltool.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QHeaderView>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QTableWidgetItem>
#include <QOpenGLFunctions>

#include <algorithm>
#include <cmath>
#include <map>

namespace {

QIcon makePrototypeThumbnail(const S_Shape2D::Polyline2D& polygon, const QColor& color, const QSize& size = QSize(70, 46))
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

    const double shapeWidth = std::max(1e-6, right - left);
    const double shapeHeight = std::max(1e-6, bottom - top);
    const double margin = 5.0;
    const double scale = std::min((size.width() - margin * 2.0) / shapeWidth,
                                  (size.height() - margin * 2.0) / shapeHeight);
    const double offsetX = (size.width() - shapeWidth * scale) * 0.5;
    const double offsetY = (size.height() - shapeHeight * scale) * 0.5;

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
    painter.setPen(QPen(color.darker(135), 2.0));
    painter.setBrush(fill);
    painter.drawPath(path);
    painter.setBrush(color.darker(130));
    painter.setPen(QPen(Qt::white, 1.0));
    painter.drawEllipse(toThumb(polygon[0]), 3.0, 3.0);
    return QIcon(pixmap);
}

}

class NestWindow::NestView : public KWCtrlView {
public:
    using Polyline = S_Polyline2D;
    struct VisualPiece {
        Polyline polygon;
        int pieceIndex = -1;
        QColor color = QColor("#4C78A8");
        QString label;
    };

    NestView(QWidget* parent = nullptr) : KWCtrlView(parent) {}

    void beginNest(const Polyline& stock, const S_Shape2D::NestScene& scene) {
        this->stock = stock;
        this->scene = scene;
        this->placed.clear();
        this->candidates.clear();
        this->nfps.clear();
        this->currentPiece.clear();
        this->utilization = 0;
        ResetView();
        update();
    }

    void addPiece(const Polyline& piece, int pieceIndex, double util) {
        VisualPiece visual;
        visual.polygon = piece;
        visual.pieceIndex = pieceIndex;
        visual.color = colorForPiece(pieceIndex);
        visual.label = labelForPiece(pieceIndex);
        this->placed.push_back(visual);
        this->candidates.clear();
        this->nfps.clear();
        this->currentPiece.clear();
        this->utilization = util;
        update();
    }

    void setPieces(const std::vector<Polyline>& pieces, double util) {
        std::vector<int> previousPieceIndices;
        previousPieceIndices.reserve(placed.size());
        for (const auto& piece : placed)
            previousPieceIndices.push_back(piece.pieceIndex);

        placed.clear();
        for (int i = 0; i < static_cast<int>(pieces.size()); ++i) {
            int pieceIndex = (i < static_cast<int>(previousPieceIndices.size())) ? previousPieceIndices[i] : i;
            VisualPiece visual;
            visual.polygon = pieces[i];
            visual.pieceIndex = pieceIndex;
            visual.color = colorForPiece(pieceIndex);
            visual.label = labelForPiece(pieceIndex);
            placed.push_back(visual);
        }
        utilization = util;
        update();
    }

    void showCandidates(const std::vector<Polyline>& cands,
                        const std::vector<Polyline>& nfpRings,
                        const Polyline& piece) {
        this->candidates = cands;
        this->nfps = nfpRings;
        this->currentPiece = piece;
        update();
    }

    int placedSize() const { return (int)placed.size(); }
    double utilizationValue() const { return utilization; }

    void ResetView() override {
        S_Shape2D::Box2D box;
        if (!stock.empty()) {
            for (const auto& pt : stock)
                box.append(pt);
        }
        for (const auto& visual : placed) {
            for (const auto& pt : visual.polygon)
                box.append(pt);
        }
        for (const auto& poly : candidates) {
            for (const auto& pt : poly)
                box.append(pt);
        }
        if (!box) {
            box.append({-100, -100});
            box.append({100, 100});
        }

        m_rect.setCoords(box.left(), box.top(), box.right(), box.bottom());
        double aspect;
        m_fScale = 1.0f;
        m_dViewW = m_rect.width() * 0.55;
        m_dViewW = (m_dViewW < 1e-6) ? 1 : m_dViewW;
        m_dViewH = m_rect.height() * 0.55;
        m_dViewH = (m_dViewH < 1e-6) ? 1 : m_dViewH;
        if (this->height() == 0)
            aspect = (GLdouble)this->width();
        else
            aspect = (GLdouble)this->width() / (GLdouble)this->height();
        m_dXTrans = -(m_rect.left() + m_rect.right()) / 2.0f;
        m_dYTrans = -(m_rect.top() + m_rect.bottom()) / 2.0f;
        if (m_dViewW < m_dViewH * aspect)
            m_dViewW = m_dViewH * aspect;
        else
            m_dViewH = m_dViewW / aspect;
        KWCtrlView::ResetView();
        update();
    }

protected:
    void DrawGLSence(QPainter& painter) override {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (!stock.empty()) {
            drawPolygon(painter, stock, QColor(230, 232, 235, 34), QColor("#D8DEE9"), 2.0);
        }

        int idx = 0;
        for (const auto& visual : placed) {
            const auto& poly = visual.polygon;
            if (poly.empty())
                continue;

            QColor fill = visual.color;
            fill.setAlpha(72);
            drawPolygon(painter, poly, fill, visual.color.darker(135), 2.0);
            drawPoint(painter, poly[0], visual.color.darker(150), 4.0);

            S_Shape2D::Box2D box = S_Shape2D::calcBoundingBox(poly);
            QPointF labelPoint(box.center().x, box.center().y);
            View2Scr(labelPoint);
            painter.setPen(QColor("#F8FAFC"));
            painter.drawText(labelPoint, QString::number(++idx));
        }

        if (!nfps.empty()) {
            for (const auto& nfp : nfps) {
                drawPolygon(painter, nfp, Qt::NoBrush, QColor("#FF4D6D"), 1.4, Qt::DashLine);
            }
        }

        if (!candidates.empty()) {
            for (const auto& cand : candidates) {
                if (!cand.empty())
                    drawPoint(painter, cand[0], QColor("#22C55E"), 2.8);
            }

            for (const auto& cand : candidates) {
                drawPolygon(painter, cand, Qt::NoBrush, QColor(34, 197, 94, 90), 1.0);
            }
        }

        if (!currentPiece.empty()) {
            drawPoint(painter, currentPiece[0], QColor("#F97316"), 5.0);
            drawPolygon(painter, currentPiece, QColor(249, 115, 22, 32), QColor("#F97316"), 2.0);
        }

        painter.setPen(Qt::white);
        QString info = QString("Pieces: %1  Utilization: %2%")
                       .arg(placed.size())
                       .arg(utilization, 0, 'f', 1);
        if (!candidates.empty())
            info += QString("  Candidates: %1").arg(candidates.size());
        painter.drawText(10, 20, info);
        painter.restore();
    }

private:
    QPointF toScreenPoint(const Polyline::Point& pt) {
        QPointF screenPoint(pt.x, pt.y);
        View2Scr(screenPoint);
        return screenPoint;
    }

    QPainterPath makePath(const Polyline& poly) {
        QPainterPath path;
        if (poly.empty())
            return path;

        path.moveTo(toScreenPoint(poly[0]));
        for (size_t i = 1; i < poly.size(); ++i)
            path.lineTo(toScreenPoint(poly[i]));
        path.closeSubpath();
        return path;
    }

    void drawPolygon(QPainter& painter, const Polyline& poly, const QBrush& fill,
                     const QColor& stroke, qreal width, Qt::PenStyle style = Qt::SolidLine) {
        if (poly.size() < 2)
            return;

        QPen pen(stroke, width, style);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(fill);
        painter.drawPath(makePath(poly));
    }

    void drawPoint(QPainter& painter, const Polyline::Point& pt, const QColor& color, qreal radius) {
        const QPointF center = toScreenPoint(pt);
        painter.setPen(QPen(Qt::white, 1.0));
        painter.setBrush(color);
        painter.drawEllipse(center, radius, radius);
    }

    QColor colorForPiece(int pieceIndex) const {
        if (pieceIndex >= 0 && pieceIndex < static_cast<int>(scene.pieces.size())) {
            const auto& piece = scene.pieces[pieceIndex];
            if (const auto* prototype = S_Shape2D::findPrototype(scene, piece.prototypeId))
                return prototype->color;
        }

        static const QColor fallback[] = {
            QColor("#4C78A8"),
            QColor("#F58518"),
            QColor("#54A24B"),
            QColor("#E45756"),
            QColor("#72B7B2"),
            QColor("#B279A2")
        };
        int idx = std::max(0, pieceIndex);
        return fallback[idx % (sizeof(fallback) / sizeof(fallback[0]))];
    }

    QString labelForPiece(int pieceIndex) const {
        if (pieceIndex >= 0 && pieceIndex < static_cast<int>(scene.pieces.size())) {
            const auto& piece = scene.pieces[pieceIndex];
            if (const auto* prototype = S_Shape2D::findPrototype(scene, piece.prototypeId))
                return prototype->name;
        }
        return QString("P%1").arg(pieceIndex + 1);
    }

    Polyline stock;
    S_Shape2D::NestScene scene;
    std::vector<VisualPiece> placed;
    std::vector<Polyline> candidates;
    std::vector<Polyline> nfps;
    Polyline currentPiece;
    double utilization = 0;
};

NestWindow::NestWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString::fromWCharArray(L"\u6392\u7248\u7ED3\u679C"));
    resize(800, 600);

    view = new NestView(this);
    lblInfo = new QLabel(this);
    lblInfo->setStyleSheet("color: white; background: #333; padding: 4px;");

    tblSummary = new QTableWidget(this);
    tblSummary->setColumnCount(4);
    tblSummary->setHorizontalHeaderLabels({
        QString::fromWCharArray(L"\u56FE\u5F62"),
        QString::fromWCharArray(L"\u6570\u91CF"),
        QString::fromWCharArray(L"\u5DF2\u653E"),
        QString::fromWCharArray(L"\u9762\u79EF")
    });
    tblSummary->verticalHeader()->setVisible(false);
    tblSummary->horizontalHeader()->setStretchLastSection(true);
    tblSummary->setSelectionMode(QAbstractItemView::NoSelection);
    tblSummary->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tblSummary->setIconSize(QSize(70, 46));
    tblSummary->setMaximumWidth(330);

    progressBar = new QProgressBar(this);
    progressBar->setStyleSheet(
        "QProgressBar { background: #222; border: 1px solid #555; height: 20px; text-align: center; color: white; }"
        "QProgressBar::chunk { background: #4CAF50; }");
    progressBar->setTextVisible(true);

    QVBoxLayout* sideLayout = new QVBoxLayout;
    sideLayout->setContentsMargins(6, 6, 6, 6);
    sideLayout->addWidget(new QLabel(QString::fromWCharArray(L"\u56FE\u5F62\u6458\u8981"), this));
    sideLayout->addWidget(tblSummary, 1);

    QHBoxLayout* contentLayout = new QHBoxLayout;
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addWidget(view, 1);
    contentLayout->addLayout(sideLayout, 0);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(contentLayout, 1);
    layout->addWidget(lblInfo, 0);
    layout->addWidget(progressBar, 0);
}

void NestWindow::updateSummaryTable()
{
    tblSummary->setRowCount(static_cast<int>(scene.prototypes.size()));
    for (int row = 0; row < static_cast<int>(scene.prototypes.size()); ++row) {
        const auto& prototype = scene.prototypes[row];

        auto* nameItem = new QTableWidgetItem(prototype.name);
        nameItem->setIcon(makePrototypeThumbnail(prototype.contour, prototype.color));
        nameItem->setSizeHint(QSize(150, 54));
        nameItem->setBackground(prototype.color);
        nameItem->setForeground(prototype.color.lightness() < 128 ? Qt::white : Qt::black);

        tblSummary->setItem(row, 0, nameItem);
        tblSummary->setItem(row, 1, new QTableWidgetItem(QString::number(prototype.quantity)));
        tblSummary->setItem(row, 2, new QTableWidgetItem(QString::number(placedPrototypeCounts[prototype.id])));
        tblSummary->setItem(row, 3, new QTableWidgetItem(QString::number(std::fabs(prototype.contour.area()), 'f', 1)));
        tblSummary->setRowHeight(row, 56);
    }
}

void NestWindow::beginNest(const Polyline& stock, int totalPieces, int saIterations)
{
    beginNest(stock, S_Shape2D::NestScene{}, totalPieces, saIterations);
}

void NestWindow::beginNest(const Polyline& stock, const S_Shape2D::NestScene& scene,
                           int totalPieces, int saIterations)
{
    this->scene = scene;
    this->totalPieces = totalPieces;
    this->saIterations = saIterations;
    this->placedCount = 0;
    this->processedPieces = 0;
    this->currentPhase = Placing;
    this->placedPrototypeCounts.clear();
    view->beginNest(stock, scene);
    updateSummaryTable();
    lblInfo->setText(QString::fromWCharArray(L"  \u6392\u7248\u4E2D... \u653E\u7F6E\u9636\u6BB5"));
    progressBar->setRange(0, 1000);
    progressBar->setValue(0);
    progressBar->setFormat(QString::fromWCharArray(L"\u653E\u7F6E\u4E2D... %p%"));
    show();
    raise();
    activateWindow();
}

void NestWindow::addPlacedPiece(const Polyline& piece, double utilization, int pieceIndex)
{
    view->addPiece(piece, pieceIndex, utilization);
    placedCount++;
    if (pieceIndex >= 0 && pieceIndex < static_cast<int>(scene.pieces.size()))
        placedPrototypeCounts[scene.pieces[pieceIndex].prototypeId]++;
    updateSummaryTable();
    if (currentPhase == Placing) {
        int visibleProgress = qMax(processedPieces, placedCount);
        int base = (totalPieces > 0) ? (visibleProgress * 600 / totalPieces) : 0;
        progressBar->setValue(qMin(base, 600));
        progressBar->setFormat(QString::fromWCharArray(L"\u653E\u7F6E\u4E2D... %p%"));
        lblInfo->setText(QString::fromWCharArray(L"  \u6392\u7248\u4E2D... \u5DF2\u653E\u7F6E %1/%2 | \u5229\u7528\u7387: %3%")
                         .arg(placedCount)
                         .arg(totalPieces)
                         .arg(utilization, 0, 'f', 1));
    } else if (currentPhase == BLF) {
        int base = 600 + (placedCount * 150 / (totalPieces > 0 ? totalPieces : 1));
        progressBar->setValue(qMin(base, 750));
        progressBar->setFormat(QString::fromWCharArray(L"BLF\u586B\u5145\u4E2D... %p%"));
        lblInfo->setText(QString::fromWCharArray(L"  BLF\u586B\u5145... \u5DF2\u653E\u7F6E %1 | \u5229\u7528\u7387: %2%")
                         .arg(placedCount)
                         .arg(utilization, 0, 'f', 1));
    }
    QCoreApplication::processEvents();
}

void NestWindow::setPlacedPieces(const std::vector<Polyline>& pieces, double utilization)
{
    view->setPieces(pieces, utilization);
    placedCount = static_cast<int>(pieces.size());
    updateSummaryTable();
    QCoreApplication::processEvents();
}

void NestWindow::setPlacementProgress(int processedPieces, int totalPieces)
{
    if (totalPieces > 0)
        this->totalPieces = totalPieces;

    this->processedPieces = qMax(0, processedPieces);

    if (currentPhase != Placing || this->totalPieces <= 0)
        return;

    int base = qMin(this->processedPieces * 600 / this->totalPieces, 600);
    progressBar->setValue(qMax(progressBar->value(), base));
    progressBar->setFormat(QString::fromWCharArray(L"\u653E\u7F6E\u4E2D... %p%"));
    lblInfo->setText(QString::fromWCharArray(L"  \u6392\u7248\u4E2D... \u5DF2\u5904\u7406 %1/%2 | \u5DF2\u653E\u7F6E %3")
                     .arg(qMin(this->processedPieces, this->totalPieces))
                     .arg(this->totalPieces)
                     .arg(placedCount));
    QCoreApplication::processEvents();
}

void NestWindow::showCandidates(const std::vector<Polyline>& candidates,
                                const std::vector<Polyline>& nfps,
                                const Polyline& currentPiece)
{
    view->showCandidates(candidates, nfps, currentPiece);
    QCoreApplication::processEvents();
}

void NestWindow::setSAProgress(int iteration, int totalIterations)
{
    int saBase = 750;
    int saRange = 250;
    int saProgress = (totalIterations > 0) ? (iteration * saRange / totalIterations) : 0;
    progressBar->setValue(saBase + saProgress);
    progressBar->setFormat(QString::fromWCharArray(L"SA\u4F18\u5316\u4E2D... %p%"));
}

void NestWindow::setPhase(const QString& phase)
{
    lblInfo->setText(phase);
    if (phase.contains(QString::fromWCharArray(L"BLF")))
        currentPhase = BLF;
    else if (phase.contains(QString::fromWCharArray(L"SA")))
        currentPhase = SA;
    else
        currentPhase = Placing;
}

void NestWindow::endNest()
{
    view->showCandidates({}, {}, {});
    progressBar->setValue(1000);
    progressBar->setFormat(QString::fromWCharArray(L"\u5B8C\u6210"));
    lblInfo->setText(QString::fromWCharArray(L"  \u6392\u7248\u5B8C\u6210 | \u4EF6\u6570: %1 | \u5229\u7528\u7387: %2% | \u53CC\u51FB\u91CD\u7F6E\u89C6\u56FE")
                     .arg(view->placedSize())
                     .arg(view->utilizationValue(), 0, 'f', 1));
}

void NestWindow::closeEvent(QCloseEvent*)
{
    hide();
}
