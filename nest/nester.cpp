#include "nester.h"
#include "nester_common.h"

#define _USE_MATH_DEFINES
#include <cmath>

#include <QDebug>
#include <algorithm>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace S_Shape2D {

Nester::Nester(QObject* parent)
    : QObject(parent)
{
}

void Nester::setStock(double width, double height)
{
    config.stockWidth = width;
    config.stockHeight = height;
}

void Nester::setPolygons(const std::vector<Polyline>& polys)
{
    polygons = polys;
    placements.clear();
}

void Nester::setConfig(const Config& cfg)
{
    config = cfg;
}

Nester::Polyline Nester::buildStockPolyline() const
{
    Polyline stock;
    stock.add({0, 0});
    stock.add({config.stockWidth, 0});
    stock.add({config.stockWidth, config.stockHeight});
    stock.add({0, config.stockHeight});
    return stock;
}

bool Nester::isInsideStock(const Polyline& poly, const Point& offset) const
{
    for (size_t i = 0; i < poly.size(); ++i) {
        Point pt = poly[i];
        pt.translate(offset);
        if (pt.x < -kNestEps || pt.x > config.stockWidth + kNestEps ||
            pt.y < -kNestEps || pt.y > config.stockHeight + kNestEps)
            return false;
    }
    return true;
}

std::vector<double> Nester::getRotationAngles() const
{
    if (!config.allowRotation)
        return {0};
    int steps = std::max(1, config.rotationSteps);
    std::vector<double> angles;
    for (int i = 0; i < steps; ++i)
        angles.push_back(2.0 * M_PI * i / steps);
    return angles;
}

double Nester::concavityMeasure(const Polyline& poly) const
{
    if (poly.size() < 3) return 0;
    Polyline copy = poly;
    double area = std::fabs(copy.area());
    Box2D bbox = calcBoundingBox(poly);
    double bboxArea = bbox.width() * bbox.height();
    if (bboxArea < 1e-12) return 0;
    return 1.0 - area / bboxArea;
}

void Nester::sortByComplexity(std::vector<int>& indices) const
{
    std::sort(indices.begin(), indices.end(), [this](int a, int b) {
        Polyline pa = polygons[a];
        Polyline pb = polygons[b];
        double areaA = std::fabs(pa.area());
        double areaB = std::fabs(pb.area());
        double concA = concavityMeasure(polygons[a]);
        double concB = concavityMeasure(polygons[b]);
        return areaA * (1.0 + concA) > areaB * (1.0 + concB);
    });
}

std::vector<Nester::Point> Nester::sampleNfpBoundary(const std::vector<Polyline>& nfps, double step) const
{
    std::vector<NestPoint> samples;
    for (const auto& nfp : nfps) {
        size_t n = nfp.size();
        if (n < 3) continue;
        for (size_t i = 0; i < n; ++i) {
            NestPoint a = nfp[i];
            NestPoint b = nfp[(i + 1) % n];
            double len = (b - a).norm();
            if (len < 1e-9) continue;
            int numSamples = std::max(1, (int)(len / step));
            for (int j = 0; j <= numSamples; ++j) {
                double t = (double)j / numSamples;
                samples.push_back({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t});
            }
        }
    }
    return samples;
}

double Nester::edgeContactLength(const Polyline& poly, const Point& offset,
                                 const std::vector<Placement>& placed) const
{
    double total = 0;
    NestPoly moved = poly;
    moved.translate(offset);
    for (const auto& p : placed) {
        NestPoly fixed = p.polygon;
        fixed.translate(p.offset);
        size_t na = moved.size();
        size_t nb = fixed.size();
        for (size_t i = 0; i < na; ++i) {
            NestPoint a0 = moved[i];
            NestPoint a1 = moved[(i + 1) % na];
            for (size_t j = 0; j < nb; ++j) {
                NestPoint b0 = fixed[j];
                NestPoint b1 = fixed[(j + 1) % nb];
                NestPoint adir = a1 - a0;
                NestPoint bdir = b1 - b0;
                double da = adir.norm();
                double db = bdir.norm();
                if (da < 1e-12 || db < 1e-12) continue;
                if (std::fabs(adir.dot(bdir) / (da * db)) < 1.0 - 1e-6) continue;
                NestPoint w = b0 - a0;
                if (std::fabs(w.cross(adir)) > 1e-6) continue;
                double t0 = (b0 - a0).dot(adir) / (da * da);
                double t1 = (b1 - a0).dot(adir) / (da * da);
                if (t0 > t1) std::swap(t0, t1);
                double os = std::max(0.0, t0);
                double oe = std::min(1.0, t1);
                if (os < oe) total += (oe - os) * da;
            }
        }
    }
    return total;
}

Nester::ScoredPosition Nester::evaluatePosition(const Polyline& poly, const Point& pos,
                                                double rotation,
                                                const std::vector<Placement>& placed) const
{
    ScoredPosition sp;
    sp.pos = pos;
    sp.rotation = rotation;
    std::vector<Placement> testPlaced = placed;
    testPlaced.push_back({poly, pos, rotation});
    Box2D bbox = computePlacedBBox(testPlaced);
    double bboxArea = bbox.width() * bbox.height();
    double contact = edgeContactLength(poly, pos, placed);
    sp.score = bboxArea - contact * 100.0;
    return sp;
}

Box2D Nester::computePlacedBBox(const std::vector<Placement>& placed) const
{
    Box2D bbox;
    for (const auto& p : placed) {
        Polyline poly = p.polygon;
        poly.translate(p.offset);
        for (const auto& pt : poly)
            bbox.append(pt);
    }
    return bbox;
}

double Nester::computePlacedArea(const std::vector<Placement>& placed) const
{
    double area = 0;
    for (const auto& p : placed) {
        Polyline poly = p.polygon;
        poly.translate(p.offset);
        area += std::fabs(poly.area());
    }
    return area;
}

std::vector<Nester::Placement> Nester::getPlacements() const
{
    return placements;
}

std::vector<Nester::Polyline> Nester::getPlacedPolygons() const
{
    std::vector<Polyline> result;
    for (const auto& p : placements) {
        Polyline poly = p.polygon;
        poly.translate(p.offset);
        result.push_back(poly);
    }
    return result;
}

Nester::Polyline Nester::getStock() const
{
    return buildStockPolyline();
}

double Nester::getUtilization() const
{
    if (placements.empty())
        return 0.0;
    double used = computePlacedArea(placements);
    double stock = config.stockWidth * config.stockHeight;
    return (stock > 0) ? (used / stock) * 100.0 : 0.0;
}

} // namespace S_Shape2D