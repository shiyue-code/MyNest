#include "nestwindow.h"
#include "kwctrlview.h"
#include "shapes/s_box.hpp"
#include "shapes/utiltool.h"

#include <QVBoxLayout>
#include <QPainter>
#include <QOpenGLFunctions>

class NestWindow::NestView : public KWCtrlView {
public:
    using Polyline = S_Polyline2D;

    NestView(QWidget* parent = nullptr) : KWCtrlView(parent) {}

    void setData(const Polyline& stock, const std::vector<Polyline>& placed, double util) {
        this->stock = stock;
        this->placed = placed;
        this->utilization = util;
        ResetView();
        update();
    }

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

        painter.save();
        painter.setPen(Qt::white);
        painter.drawText(10, 20, QString("Pieces: %1  Utilization: %2%")
                         .arg(placed.size())
                         .arg(utilization, 0, 'f', 1));
        painter.restore();
    }

private:
    Polyline stock;
    std::vector<Polyline> placed;
    double utilization = 0;
};

NestWindow::NestWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString::fromUtf8("排版结果"));
    resize(800, 600);

    view = new NestView(this);
    lblInfo = new QLabel(this);
    lblInfo->setStyleSheet("color: white; background: #333; padding: 4px;");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(view, 1);
    layout->addWidget(lblInfo, 0);
}

void NestWindow::setNestResult(const Polyline& stock,
                               const std::vector<Polyline>& placed,
                               double utilization)
{
    view->setData(stock, placed, utilization);
    lblInfo->setText(QString::fromUtf8("  排版件数: %1 | 利用率: %2% | 双击重置视图")
                     .arg(placed.size())
                     .arg(utilization, 0, 'f', 1));
}

void NestWindow::closeEvent(QCloseEvent*)
{
    hide();
}
