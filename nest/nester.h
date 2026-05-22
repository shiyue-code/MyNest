#ifndef NESTER_H
#define NESTER_H

#include "nest_scene.h"
#include "nfp_placer.h"
#include "shapes/s_box.hpp"
#include <QObject>
#include <vector>
#include <functional>
#include <random>
#include <future>

namespace S_Shape2D {

class Nester : public QObject {
    Q_OBJECT

public:
    using Polyline = S_Polyline2D;
    using Point = Polyline::Point;
    using Coord = Point::Coord;

    struct Placement {
        Polyline polygon;
        Point offset;
        double rotation = 0;
        int pieceIndex = -1;
        int pieceId = -1;
        int prototypeId = -1;
        QString prototypeName;
        QColor color;
    };

    struct Config {
        double stockWidth = 500.0;
        double stockHeight = 500.0;
        int nfpMethod = 1;
        bool allowRotation = true;
        int rotationSteps = 24;
        bool enableBacktrack = true;
        int backtrackDepth = 3;
        bool enableBLF = true;
        bool enableSA = true;
        int saIterations = 500;
        int saCandidateLimit = 80;
        int saSlideCandidateLimit = 20;
        int saUpdateInterval = 50;
    };

    explicit Nester(QObject* parent = nullptr);

    void setStock(double width, double height);
    void setPolygons(const std::vector<Polyline>& polys);
    void setScene(const NestScene& newScene);
    void setConfig(const Config& newConfig);

    void execBL();
    void execGreedy();

    const NestScene& getScene() const;
    std::vector<Placement> getPlacements() const;
    std::vector<Polyline> getPlacedPolygons() const;
    Polyline getStock() const;
    double getUtilization() const;

signals:
    void stepCompleted(const std::vector<Polyline>& placed,
                       const Polyline& stock,
                       double utilization,
                       int pieceIndex);
    void candidatesReady(const std::vector<Polyline>& candidates,
                         const std::vector<Polyline>& nfps,
                         const Polyline& currentPiece);
    void placementProgress(int processedPieces, int totalPieces);
    void saProgress(int iteration, int totalIterations);
    void phaseChanged(const QString& phase);
    void finished();

private:
    struct ScoredPosition {
        Point pos;
        double rotation = 0;
        double score = 1e18;
        std::vector<Polyline> candidatePolys;
        std::vector<Polyline> nfpPolys;
    };

    Polyline buildStockPolyline() const;
    bool isInsideStock(const Polyline& poly, const Point& offset) const;
    std::vector<double> getRotationAngles() const;

    double concavityMeasure(const Polyline& poly) const;
    void sortByComplexity(std::vector<int>& indices) const;

    ScoredPosition evaluatePosition(const Polyline& poly, const Point& pos,
                                    double rotation, const std::vector<Placement>& placed) const;
    double edgeContactLength(const Polyline& poly, const Point& offset,
                             const std::vector<Placement>& placed) const;

    Box2D computePlacedBBox(const std::vector<Placement>& placed) const;
    double computePlacedArea(const std::vector<Placement>& placed) const;
    Placement makePlacement(int polygonIndex, const Polyline& poly, const Point& offset, double rotation) const;

    ScoredPosition evaluateRotation(const Polyline& poly, double rot,
                                    const std::vector<Placement>& placed,
                                    bool useBL,
                                    bool collectPreview = true);

    std::vector<Point> sampleNfpBoundary(const std::vector<Polyline>& nfps, double step) const;
    std::vector<Point> blfFill(const Polyline& poly, const std::vector<Placement>& placed) const;
    bool backtrackPlace(std::vector<int>& order, int depth, std::vector<Placement>& result,
                        bool useBL);
    void simulatedAnnealing(std::vector<Placement>& placements, int iterations);

    std::vector<Polyline> polygons;
    std::vector<Placement> placements;
    NestScene scene;
    std::vector<PieceInstance> pieceInstances;
    Config config;
    mutable std::mt19937 rng{std::random_device{}()};
};

}

#endif // NESTER_H
