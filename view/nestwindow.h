#ifndef NESTWINDOW_H
#define NESTWINDOW_H

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <vector>
#include "shapes/s_polyline.hpp"

class KWCtrlView;

class NestWindow : public QDialog {
    Q_OBJECT
public:
    using Polyline = S_Polyline2D;

    explicit NestWindow(QWidget* parent = nullptr);

    void beginNest(const Polyline& stock, int totalPieces, int saIterations);
    void addPlacedPiece(const Polyline& piece, double utilization);
    void showCandidates(const std::vector<Polyline>& candidates,
                        const std::vector<Polyline>& nfps,
                        const Polyline& currentPiece);
    void endNest();

    void setSAProgress(int iteration, int totalIterations);
    void setPhase(const QString& phase);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    class NestView;
    NestView* view;
    QLabel* lblInfo;
    QProgressBar* progressBar;

    int totalPieces = 0;
    int saIterations = 0;
    int placedCount = 0;
    enum Phase { Placing, BLF, SA } currentPhase = Placing;
};

#endif // NESTWINDOW_H
