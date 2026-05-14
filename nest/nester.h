#ifndef NESTER_H
#define NESTER_H

#include "nfpplacer.h"
#include "shapes/s_box.hpp"
#include <vector>
#include <thread>
#include <future>

namespace S_Shape2D {

class Nester {
public:
    using Polyline = S_Polyline2D;
    using Point = Polyline::Point;
    using Coord = Point::Coord;

    struct Placement {
        Polyline polygon;
        Point offset;
        double rotation = 0;
    };

    struct Config {
        double stockWidth = 500.0;
        double stockHeight = 500.0;
        int nfpMethod = 1;
        bool allowRotation = true;
    };

    Nester() = default;

    void setStock(double width, double height);
    void setPolygons(const std::vector<Polyline>& polys);
    void setConfig(const Config& cfg);

    void execBL();
    void execGreedy();

    std::vector<Placement> getPlacements() const;
    std::vector<Polyline> getPlacedPolygons() const;
    Polyline getStock() const;
    double getUtilization() const;

private:
    struct ScoredPosition {
        Point pos;
        double rotation = 0;
        double score = 1e18;
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

    ScoredPosition evaluateRotation(const Polyline& poly, double rot,
                                    const std::vector<Placement>& placed,
                                    bool useBL) const;

    std::vector<Polyline> polygons;
    std::vector<Placement> placements;
    Config config;
};

}

#endif // NESTER_H
