#ifndef NESTCANVASVIEW_H
#define NESTCANVASVIEW_H

#include "kwctrlview.h"
#include "nest/nest_scene.h"
#include "shapes/s_polyline.hpp"

#include <QBrush>
#include <QPainterPath>

#include <vector>

class NestCanvasView : public KWCtrlView {
public:
    using Polyline = S_Polyline2D;

    explicit NestCanvasView(QWidget* parent = nullptr);

    void beginNest(const Polyline& stock, const S_Shape2D::NestScene& scene);
    void addPiece(const Polyline& piece, int pieceIndex, double utilization);
    void setPieces(const std::vector<Polyline>& pieces, double utilization);
    void showCandidates(const std::vector<Polyline>& candidates,
                        const std::vector<Polyline>& nfps,
                        const Polyline& currentPiece);

    int placedSize() const;
    double utilizationValue() const;

    void ResetView() override;

protected:
    void DrawGLSence(QPainter& painter) override;

private:
    struct VisualPiece {
        Polyline polygon;
        int pieceIndex = -1;
        QColor color = QColor("#4C78A8");
        QString label;
    };

    QPointF toScreenPoint(const Polyline::Point& pt);
    QPainterPath makePath(const Polyline& poly);
    void drawPolygon(QPainter& painter, const Polyline& poly, const QBrush& fill,
                     const QColor& stroke, qreal width, Qt::PenStyle style = Qt::SolidLine);
    void drawPoint(QPainter& painter, const Polyline::Point& pt, const QColor& color, qreal radius);
    QColor colorForPiece(int pieceIndex) const;
    QString labelForPiece(int pieceIndex) const;

    Polyline stock;
    S_Shape2D::NestScene scene;
    std::vector<VisualPiece> placed;
    std::vector<Polyline> candidates;
    std::vector<Polyline> nfps;
    Polyline currentPiece;
    double utilization = 0;
};

#endif // NESTCANVASVIEW_H
