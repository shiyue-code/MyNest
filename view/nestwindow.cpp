#include "nestwindow.h"
#include "kwctrlview.h"
#include "shapes/s_box.hpp"
#include "shapes/utiltool.h"

#include <QVBoxLayout>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QPainter>
#include <QOpenGLFunctions>

class NestWindow::NestView : public KWCtrlView {
public:
    using Polyline = S_Polyline2D;

    NestView(QWidget* parent = nullptr) : KWCtrlView(parent) {}

    void beginNest(const Polyline& stock) {
        this->stock = stock;
        this->placed.clear();
        this->candidates.clear();
        this->nfps.clear();
        this->currentPiece.clear();
        this->utilization = 0;
        ResetView();
        update();
    }

    void addPiece(const Polyline& piece, double util) {
        this->placed.push_back(piece);
        this->candidates.clear();
        this->nfps.clear();
        this->currentPiece.clear();
        this->utilization = util;
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
        for (const auto& poly : placed) {
            for (const auto& pt : poly)
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
        if (!stock.empty()) {
            glColor3f(1.0f, 1.0f, 0.0f);
            glLineWidth(2.0f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glBegin(GL_POLYGON);
            for (const auto& pt : stock)
                glVertex2d(pt.x, pt.y);
            glEnd();
        }

        int idx = 0;
        for (const auto& poly : placed) {
            if (idx == 0) glColor3f(0.0f, 1.0f, 1.0f);
            else if (idx == 1) glColor3f(0.5f, 0.5f, 1.0f);
            else if (idx == 2) glColor3f(1.0f, 0.5f, 0.0f);
            else if (idx == 3) glColor3f(0.5f, 1.0f, 0.5f);
            else glColor3f(1.0f, 0.5f, 1.0f);
            idx++;

            glPointSize(5);
            glBegin(GL_POINTS);
            glVertex2d(poly[0].x, poly[0].y);
            glEnd();

            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glBegin(GL_POLYGON);
            for (const auto& pt : poly)
                glVertex2d(pt.x, pt.y);
            glEnd();
        }

        if (!nfps.empty()) {
            glColor3f(0.4f, 0.4f, 0.4f);
            glLineWidth(1.0f);
            glLineStipple(1, 0x00FF);
            glEnable(GL_LINE_STIPPLE);
            for (const auto& nfp : nfps) {
                glBegin(GL_LINE_LOOP);
                for (const auto& pt : nfp)
                    glVertex2d(pt.x, pt.y);
                glEnd();
            }
            glDisable(GL_LINE_STIPPLE);
        }

        if (!candidates.empty()) {
            glColor3f(0.3f, 0.6f, 0.3f);
            glPointSize(3);
            glBegin(GL_POINTS);
            for (const auto& cand : candidates) {
                if (!cand.empty())
                    glVertex2d(cand[0].x, cand[0].y);
            }
            glEnd();

            for (const auto& cand : candidates) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glBegin(GL_LINE_LOOP);
                for (const auto& pt : cand)
                    glVertex2d(pt.x, pt.y);
                glEnd();
            }
        }

        if (!currentPiece.empty()) {
            glColor3f(1.0f, 0.0f, 0.0f);
            glPointSize(6);
            glBegin(GL_POINTS);
            glVertex2d(currentPiece[0].x, currentPiece[0].y);
            glEnd();

            glLineWidth(2.0f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glBegin(GL_POLYGON);
            for (const auto& pt : currentPiece)
                glVertex2d(pt.x, pt.y);
            glEnd();
        }

        painter.save();
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
    Polyline stock;
    std::vector<Polyline> placed;
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

    progressBar = new QProgressBar(this);
    progressBar->setStyleSheet(
        "QProgressBar { background: #222; border: 1px solid #555; height: 20px; text-align: center; color: white; }"
        "QProgressBar::chunk { background: #4CAF50; }");
    progressBar->setTextVisible(true);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(view, 1);
    layout->addWidget(lblInfo, 0);
    layout->addWidget(progressBar, 0);
}

void NestWindow::beginNest(const Polyline& stock, int totalPieces, int saIterations)
{
    this->totalPieces = totalPieces;
    this->saIterations = saIterations;
    this->placedCount = 0;
    view->beginNest(stock);
    lblInfo->setText(QString::fromWCharArray(L"  \u6392\u7248\u4E2D... \u653E\u7F6E\u9636\u6BB5"));
    progressBar->setRange(0, 1000);
    progressBar->setValue(0);
    progressBar->setFormat(QString::fromWCharArray(L"\u653E\u7F6E\u4E2D... %p%"));
    show();
    raise();
    activateWindow();
}

void NestWindow::addPlacedPiece(const Polyline& piece, double utilization)
{
    view->addPiece(piece, utilization);
    placedCount++;
    if (currentPhase == Placing) {
        int base = (totalPieces > 0) ? (placedCount * 600 / totalPieces) : 0;
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
