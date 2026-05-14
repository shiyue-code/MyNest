#include "nester.h"
#include "nfpplacer_common.h"
#include "shapes/utiltool.h"

#define _USE_MATH_DEFINES
#include <cmath>

#include <QDebug>
#include <QElapsedTimer>
#include <algorithm>
#include <limits>
#include <future>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace S_Shape2D {

using NestPoly = S_Polyline2D;
using NestPoint = S_Point2D;

static constexpr double EPS = 1e-6;

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

static double ptSegDist(const NestPoint& p, const NestPoint& a, const NestPoint& b)
{
    NestPoint ab = b - a;
    NestPoint ap = p - a;
    double len2 = ab.square();
    if (len2 < 1e-16)
        return std::sqrt(ap.square());
    double t = ap.dot(ab) / len2;
    t = std::max(0.0, std::min(1.0, t));
    NestPoint proj = a + ab * t;
    return std::sqrt((p - proj).square());
}

static bool ptInside(const NestPoint& pt, const NestPoly& poly)
{
    int c = 0;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const auto& vi = poly[i];
        const auto& vj = poly[j];
        if (((vi.y > pt.y) != (vj.y > pt.y)) &&
            (pt.x < (vj.x - vi.x) * (pt.y - vi.y) / (vj.y - vi.y) + vi.x))
            c++;
    }
    return (c & 1) != 0;
}

bool Nester::isInsideStock(const Polyline& poly, const Point& offset) const
{
    for (size_t i = 0; i < poly.size(); ++i) {
        Point pt = poly[i];
        pt.translate(offset);
        if (pt.x < -EPS || pt.x > config.stockWidth + EPS ||
            pt.y < -EPS || pt.y > config.stockHeight + EPS)
            return false;
    }
    return true;
}

static bool isOutsideAllNfps(const NestPoint& pt, const std::vector<NestPoly>& nfps)
{
    ClipperLib::IntPoint cp;
    cp.X = static_cast<ClipperLib::cInt>(std::llround(pt.x * clipperScaler));
    cp.Y = static_cast<ClipperLib::cInt>(std::llround(pt.y * clipperScaler));

    for (const auto& nfp : nfps) {
        if (nfp.size() < 3) continue;
        ClipperLib::Path path = polygon2Path(nfp);
        int result = ClipperLib::PointInPolygon(cp, path);
        if (result == 1)
            return false;
    }
    return true;
}

std::vector<double> Nester::getRotationAngles() const
{
    if (config.allowRotation)
        return {0, M_PI / 2, M_PI, 3 * M_PI / 2};
    return {0};
}

double Nester::concavityMeasure(const Polyline& poly) const
{
    if (poly.size() < 3)
        return 0;
    Polyline copy = poly;
    double area = std::fabs(copy.area());
    Box2D bbox = calcBoundingBox(poly);
    double bboxArea = bbox.width() * bbox.height();
    if (bboxArea < 1e-12)
        return 0;
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

static std::vector<NestPoly> computeNfpsForMoving(const NestPoly& moving,
                                                   const std::vector<Nester::Placement>& placed,
                                                   int /*method*/)
{
    std::vector<NestPoly> result;
    for (const auto& p : placed) {
        NestPoly fixed = p.polygon;
        fixed.translate(p.offset);
        NfpPlacer placer(fixed, moving);
        placer.execMinkowski();
        auto nfps = placer.getNFPs();
        for (const auto& nfp : nfps) {
            if (nfp.size() >= 3)
                result.push_back(nfp);
        }
    }
    return result;
}

static std::vector<NestPoint> getNfpVertices(const NestPoly& fixed, const NestPoly& moving, int /*method*/)
{
    std::vector<NestPoint> result;
    NfpPlacer placer(fixed, moving);
    placer.execMinkowski();
    auto nfps = placer.getNFPs();
    for (const auto& nfp : nfps)
        for (const auto& pt : nfp)
            result.push_back(pt);
    return result;
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
        if (std::fabs(p.rotation) > 1e-9)
            poly.rotate(p.rotation);
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
        if (std::fabs(p.rotation) > 1e-9)
            poly.rotate(p.rotation);
        poly.translate(p.offset);
        area += std::fabs(poly.area());
    }
    return area;
}

static NestPoint blSlide(const NestPoly& poly, NestPoint pos,
                         const std::vector<NestPoly>& nfps,
                         double stockW, double stockH)
{
    Box2D bbox = calcBoundingBox(poly);
    double step = std::max(10.0, std::min(bbox.width(), bbox.height()) * 0.2);

    for (int iter = 0; iter < 15; ++iter) {
        NestPoint best = pos;
        bool improved = false;

        double dyArr[3] = {-step, 0, step};
        double dxArr[3] = {-step, 0, step};
        for (int yi = 0; yi < 3; ++yi) {
            for (int xi = 0; xi < 3; ++xi) {
                double dx = dxArr[xi];
                double dy = dyArr[yi];
                if (dx == 0 && dy == 0) continue;
                NestPoint cand = {pos.x + dx, pos.y + dy};
                if (cand.x < -EPS || cand.y < -EPS) continue;
                if (cand.x > stockW + EPS || cand.y > stockH + EPS) continue;
                if (!isOutsideAllNfps(cand, nfps)) continue;
                if (cand.y < best.y - EPS || (std::fabs(cand.y - best.y) < EPS && cand.x < best.x)) {
                    best = cand;
                    improved = true;
                }
            }
        }
        if (!improved) break;
        pos = best;
        step = std::max(3.0, step * 0.5);
    }
    return pos;
}

Nester::ScoredPosition Nester::evaluateRotation(const Polyline& poly, double rot,
                                                const std::vector<Placement>& placed,
                                                bool useBL) const
{
    QElapsedTimer t;
    ScoredPosition bestSP;
    bestSP.score = 1e18;
    bestSP.rotation = rot;

    NestPoly rotated = poly;
    if (std::fabs(rot) > 1e-9)
        rotated.rotate(rot);

    t.start();
    auto nfps = computeNfpsForMoving(rotated, placed, config.nfpMethod);
    qint64 nfpMs = t.elapsed();

    for (size_t i = 0; i < nfps.size(); ++i) {
        Box2D nfpBox;
        for (const auto& pt : nfps[i]) nfpBox.append(pt);
        qDebug() << "      nfp" << i << "verts:" << nfps[i].size()
                 << "bbox:" << nfpBox.left() << nfpBox.top() << nfpBox.right() << nfpBox.bottom();

        for (const auto& p : placed) {
            ClipperLib::IntPoint ref;
            ref.X = static_cast<ClipperLib::cInt>(std::llround(p.offset.x * clipperScaler));
            ref.Y = static_cast<ClipperLib::cInt>(std::llround(p.offset.y * clipperScaler));
            ClipperLib::Path path = polygon2Path(nfps[i]);
            int pir = ClipperLib::PointInPolygon(ref, path);
            qDebug() << "        placed ref (" << p.offset.x << p.offset.y << ") in nfp:" << pir;
        }
    }

    Box2D movingBox = calcBoundingBox(rotated);
    qDebug() << "      moving bbox:" << movingBox.left() << movingBox.top()
             << movingBox.right() << movingBox.bottom();

    t.start();
    std::vector<NestPoint> candidates;
    candidates.push_back({0, 0});

    for (const auto& p : placed) {
        NestPoly fixed = p.polygon;
        fixed.translate(p.offset);
        auto nfpC = getNfpVertices(fixed, rotated, config.nfpMethod);
        candidates.insert(candidates.end(), nfpC.begin(), nfpC.end());
    }

    Box2D bb = calcBoundingBox(rotated);
    double pw = bb.width();
    double ph = bb.height();
    for (double y = 0; y <= config.stockHeight - ph + EPS; y += std::max(10.0, ph * 0.3)) {
        for (double x = 0; x <= config.stockWidth - pw + EPS; x += std::max(10.0, pw * 0.3)) {
            candidates.push_back({x, y});
        }
    }
    qint64 candMs = t.elapsed();

    qint64 checkMs = 0, slideMs = 0;
    int rejected = 0, validCount = 0;

    t.start();
    std::vector<NestPoint> validCands;
    for (auto& cand : candidates) {
        if (!isInsideStock(rotated, cand)) continue;
        if (!isOutsideAllNfps(cand, nfps)) { rejected++; continue; }
        validCands.push_back(cand);
    }
    checkMs = t.elapsed();

    if (useBL) {
        std::sort(validCands.begin(), validCands.end(), [](const NestPoint& a, const NestPoint& b) {
            double da = a.y * 10000 + a.x;
            double db = b.y * 10000 + b.x;
            return da < db;
        });

        int slideLimit = std::min((int)validCands.size(), 50);
        t.start();
        for (int i = 0; i < slideLimit; ++i) {
            NestPoint cand = validCands[i];
            NestPoint finalPos = blSlide(rotated, cand, nfps, config.stockWidth, config.stockHeight);
            if (!isInsideStock(rotated, finalPos)) continue;
            if (!isOutsideAllNfps(finalPos, nfps)) continue;

            ScoredPosition sp = evaluatePosition(rotated, finalPos, rot, placed);
            validCount++;
            if (sp.score < bestSP.score) {
                bestSP = sp;
                bestSP.rotation = rot;
            }
        }
        slideMs = t.elapsed();
    } else {
        t.start();
        for (auto& cand : validCands) {
            ScoredPosition sp = evaluatePosition(rotated, cand, rot, placed);
            validCount++;
            if (sp.score < bestSP.score) {
                bestSP = sp;
                bestSP.rotation = rot;
            }
        }
        slideMs = t.elapsed();
    }

    qDebug() << "    rot" << (rot * 180 / M_PI) << "°"
             << "| NFP:" << nfpMs << "ms (" << nfps.size() << "rings)"
             << "| cands:" << candMs << "ms (" << candidates.size() << ")"
             << "| check:" << checkMs << "ms (" << rejected << "rej)"
             << "| slide:" << slideMs << "ms"
             << "| valid:" << validCount
             << "| best:" << bestSP.pos.x << bestSP.pos.y << "score:" << bestSP.score;

    return bestSP;
}

void Nester::execBL()
{
    QElapsedTimer timer;
    timer.start();
    placements.clear();

    std::vector<int> indices(polygons.size());
    for (size_t i = 0; i < indices.size(); ++i)
        indices[i] = i;
    sortByComplexity(indices);

    qDebug() << "BL sorted:" << indices;

    for (int idx : indices) {
        Polyline poly = polygons[idx];
        cleanPolygon(poly);
        if (poly.size() < 3) continue;
        if (poly.orientation() == Polyline::Clockwise)
            poly.reverse();

        auto rotations = getRotationAngles();
        std::vector<std::future<ScoredPosition>> futures;
        futures.reserve(rotations.size());

        for (double rot : rotations) {
            auto placementsCopy = placements;
            futures.push_back(std::async(std::launch::async,
                [this, poly, rot, placementsCopy]() {
                    return evaluateRotation(poly, rot, placementsCopy, true);
                }));
        }

        ScoredPosition bestSP;
        bestSP.score = 1e18;
        bool found = false;

        for (auto& f : futures) {
            ScoredPosition sp = f.get();
            if (sp.score < bestSP.score) {
                bestSP = sp;
                found = true;
            }
        }

        if (found) {
            Polyline placedPoly = poly;
            if (std::fabs(bestSP.rotation) > 1e-9)
                placedPoly.rotate(bestSP.rotation);
            placements.push_back({placedPoly, bestSP.pos, bestSP.rotation});

            Box2D totalBbox = computePlacedBBox(placements);
            double util = computePlacedArea(placements) / (config.stockWidth * config.stockHeight) * 100.0;
            qDebug() << "BL placed" << idx << "at" << bestSP.pos.x << bestSP.pos.y
                     << "rot" << (bestSP.rotation * 180 / M_PI)
                     << "| bbox:" << totalBbox.left() << totalBbox.top()
                     << totalBbox.right() << totalBbox.bottom()
                     << "| util:" << util << "%";
        } else {
            qDebug() << "BL skipped" << idx;
        }
    }

    qDebug() << "BL took" << timer.elapsed() << "ms placed" << placements.size() << "/" << polygons.size();
}

void Nester::execGreedy()
{
    QElapsedTimer timer;
    timer.start();
    placements.clear();

    std::vector<int> indices(polygons.size());
    for (size_t i = 0; i < indices.size(); ++i)
        indices[i] = i;
    sortByComplexity(indices);

    qDebug() << "Greedy sorted:" << indices;

    for (int idx : indices) {
        Polyline poly = polygons[idx];
        cleanPolygon(poly);
        if (poly.size() < 3) continue;
        if (poly.orientation() == Polyline::Clockwise)
            poly.reverse();

        auto rotations = getRotationAngles();
        std::vector<std::future<ScoredPosition>> futures;
        futures.reserve(rotations.size());

        for (double rot : rotations) {
            auto placementsCopy = placements;
            futures.push_back(std::async(std::launch::async,
                [this, poly, rot, placementsCopy]() {
                    return evaluateRotation(poly, rot, placementsCopy, false);
                }));
        }

        ScoredPosition bestSP;
        bestSP.score = 1e18;
        bool found = false;

        for (auto& f : futures) {
            ScoredPosition sp = f.get();
            if (sp.score < bestSP.score) {
                bestSP = sp;
                found = true;
            }
        }

        if (found) {
            Polyline placedPoly = poly;
            if (std::fabs(bestSP.rotation) > 1e-9)
                placedPoly.rotate(bestSP.rotation);
            placements.push_back({placedPoly, bestSP.pos, bestSP.rotation});

            Box2D totalBbox = computePlacedBBox(placements);
            double util = computePlacedArea(placements) / (config.stockWidth * config.stockHeight) * 100.0;
            qDebug() << "Greedy placed" << idx << "at" << bestSP.pos.x << bestSP.pos.y
                     << "rot" << (bestSP.rotation * 180 / M_PI)
                     << "| bbox:" << totalBbox.left() << totalBbox.top()
                     << totalBbox.right() << totalBbox.bottom()
                     << "| util:" << util << "%";
        } else {
            qDebug() << "Greedy skipped" << idx;
        }
    }

    qDebug() << "Greedy took" << timer.elapsed() << "ms placed" << placements.size() << "/" << polygons.size();
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

}
