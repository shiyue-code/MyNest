#ifndef NESTWINDOW_H
#define NESTWINDOW_H

#include <QDialog>
#include <QLabel>
#include <vector>
#include "shapes/s_polyline.hpp"

class KWCtrlView;

class NestWindow : public QDialog {
    Q_OBJECT
public:
    using Polyline = S_Polyline2D;

    explicit NestWindow(QWidget* parent = nullptr);

    void beginNest(const Polyline& stock);
    void addPlacedPiece(const Polyline& piece, double utilization);
    void showCandidates(const std::vector<Polyline>& candidates,
                        const std::vector<Polyline>& nfps,
                        const Polyline& currentPiece);
    void endNest();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    class NestView;
    NestView* view;
    QLabel* lblInfo;
};

#endif // NESTWINDOW_H
