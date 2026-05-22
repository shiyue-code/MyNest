#ifndef NESTWINDOW_H
#define NESTWINDOW_H

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <map>
#include <vector>
#include "nest/nest_scene.h"
#include "shapes/s_polyline.hpp"

class KWCtrlView;

class NestWindow : public QDialog {
    Q_OBJECT
public:
    using Polyline = S_Polyline2D;

    explicit NestWindow(QWidget* parent = nullptr);

    void beginNest(const Polyline& stock, int totalPieces, int saIterations);
    void beginNest(const Polyline& stock, const S_Shape2D::NestScene& scene,
                   int totalPieces, int saIterations);
    void addPlacedPiece(const Polyline& piece, double utilization, int pieceIndex = -1);
    void setPlacedPieces(const std::vector<Polyline>& pieces, double utilization);
    void showCandidates(const std::vector<Polyline>& candidates,
                        const std::vector<Polyline>& nfps,
                        const Polyline& currentPiece);
    void endNest();

    void setPlacementProgress(int processedPieces, int totalPieces);
    void setSAProgress(int iteration, int totalIterations);
    void setPhase(const QString& phase);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void updateSummaryTable();

    class NestView;
    NestView* view;
    QLabel* lblInfo;
    QTableWidget* tblSummary;
    QProgressBar* progressBar;
    S_Shape2D::NestScene scene;
    std::map<int, int> placedPrototypeCounts;

    int totalPieces = 0;
    int saIterations = 0;
    int placedCount = 0;
    int processedPieces = 0;
    enum Phase { Placing, BLF, SA } currentPhase = Placing;
};

#endif // NESTWINDOW_H
