#include "nester.h"
#include "nfpplacer_common.h"
#include "shapes/utiltool.h"

#define _USE_MATH_DEFINES
#include <cmath>

#include <QDebug>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <algorithm>
#include <limits>
#include <future>
#include <numeric>

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

void Nester::setStepCallback(StepCallback cb)
{
    stepCallback = std::move(cb);
}

void Nester::notifyStep(int pieceIndex)
{
    if (stepCallback) {
        auto placed = getPlacedPolygons();
        auto stock = getStock();
        double util = getUtilization();
        stepCallback(placed, stock, util, pieceIndex);
    }
}

void Nester::setCandidateCallback(CandidateCallback cb)
{
    candidateCallback = std::move(cb);
}

void Nester::notifyCandidates(const std::vector<Polyline>& candidates,
                              const std::vector<Polyline>& nfps,
                              const Polyline& currentPiece)
{
    if (candidateCallback)
        candidateCallback(candidates, nfps, currentPiece);
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

static bool segsCross(const NestPoint& a0, const NestPoint& a1,
                      const NestPoint& b0, const NestPoint& b1)
{
    NestPoint r = a1 - a0;
    NestPoint s = b1 - b0;
    double d = r.cross(s);
    if (std::fabs(d) < 1e-12) return false;
    NestPoint qmb = b0 - a0;
    double t = qmb.cross(s) / d;
    double u = qmb.cross(r) / d;
    return t > 1e-9 && t < 1.0 - 1e-9 && u > 1e-9 && u < 1.0 - 1e-9;
}

static bool polysReallyOverlap(const NestPoly& a, const NestPoly& b)
{
    size_t na = a.size();
    size_t nb = b.size();
    for (size_t i = 0; i < na; ++i) {
        NestPoint a0 = a[i];
        NestPoint a1 = a[(i + 1) % na];
        for (size_t j = 0; j < nb; ++j) {
            if (segsCross(a0, a1, b[j], b[(j + 1) % nb]))
                return true;
        }
    }
    if (ptInside(a[0], b)) return true;
    if (ptInside(b[0], a)) return true;
    return false;
}

static bool overlapsAnyPlaced(const NestPoly& poly, const std::vector<Nester::Placement>& placed)
{
    for (const auto& p : placed) {
        NestPoly fixed = p.polygon;
        fixed.translate(p.offset);
        if (polysReallyOverlap(fixed, poly))
            return true;
    }
    return false;
}

struct NfpGroup {
    std::vector<NestPoly> outers;
    std::vector<NestPoly> holes;
};

static double signedArea2x(const NestPoly& poly)
{
    double area = 0;
    size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        const auto& a = poly[i];
        const auto& b = poly[(i + 1) % n];
        area += a.x * b.y - b.x * a.y;
    }
    return area;
}

static std::vector<NfpGroup> computeNfpsForMoving(const NestPoly& moving,
                                                   const std::vector<Nester::Placement>& placed,
                                                   int /*method*/)
{
    std::vector<NfpGroup> result;
    NestPoint refOffset = moving[0];

    for (const auto& p : placed) {
        NestPoly fixed = p.polygon;
        fixed.translate(p.offset);
        NfpPlacer placer(fixed, moving);
        placer.execMinkowski();
        auto nfps = placer.getNFPs();

        NfpGroup group;
        for (const auto& nfp : nfps) {
            if (nfp.size() < 3) continue;
            NestPoly corrected = nfp;
            for (auto& pt : corrected) {
                pt.x -= refOffset.x;
                pt.y -= refOffset.y;
            }
            double a2x = signedArea2x(corrected);
            if (a2x > 0)
                group.outers.push_back(corrected);
            else if (a2x < 0)
                group.holes.push_back(corrected);
        }
        if (!group.outers.empty())
            result.push_back(std::move(group));
    }
    return result;
}

static bool isOutsideAllNfps(const NestPoint& pt, const std::vector<NfpGroup>& nfpGroups)
{
    if (nfpGroups.empty())
        return true;

    ClipperLib::IntPoint cp;
    cp.X = static_cast<ClipperLib::cInt>(std::llround(pt.x * clipperScaler));
    cp.Y = static_cast<ClipperLib::cInt>(std::llround(pt.y * clipperScaler));

    for (const auto& group : nfpGroups) {
        bool insideOuter = false;
        for (const auto& outer : group.outers) {
            ClipperLib::Path path = polygon2Path(outer);
            if (ClipperLib::PointInPolygon(cp, path) == 1) {
                insideOuter = true;
                break;
            }
        }
        if (!insideOuter) continue;

        bool insideHole = false;
        for (const auto& hole : group.holes) {
            ClipperLib::Path path = polygon2Path(hole);
            if (ClipperLib::PointInPolygon(cp, path) == 1) {
                insideHole = true;
                break;
            }
        }

        if (insideOuter && !insideHole)
            return false;
    }
    return true;
}

static std::vector<NestPoly> flattenNfpGroups(const std::vector<NfpGroup>& groups)
{
    std::vector<NestPoly> result;
    for (const auto& g : groups) {
        for (const auto& o : g.outers) result.push_back(o);
        for (const auto& h : g.holes) result.push_back(h);
    }
    return result;
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

std::vector<NestPoint> Nester::sampleNfpBoundary(const std::vector<Polyline>& nfps, double step) const
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

static NestPoint blSlide(const NestPoly& poly, NestPoint pos,
                         const std::vector<NfpGroup>& nfpGroups,
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
                if (!isOutsideAllNfps(cand, nfpGroups)) continue;
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
                                                bool useBL)
{
    NestPoly rotated = poly;
    if (std::fabs(rot) > 1e-9)
        rotated.rotate(rot);

    if (rotated.orientation() == NestPoly::Clockwise)
        rotated.reverse();

    auto nfps = computeNfpsForMoving(rotated, placed, config.nfpMethod);
    auto flatNfps = flattenNfpGroups(nfps);

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
    double nfpStep = std::max(5.0, std::min(pw, ph) * 0.3);
    auto nfpSamples = sampleNfpBoundary(flatNfps, nfpStep);
    candidates.insert(candidates.end(), nfpSamples.begin(), nfpSamples.end());

    ScoredPosition bestSP;
    bestSP.score = 1e18;
    bestSP.rotation = rot;

    std::vector<NestPoint> validCands;
    for (auto& cand : candidates) {
        if (!isInsideStock(rotated, cand)) continue;
        if (!isOutsideAllNfps(cand, nfps)) continue;
        validCands.push_back(cand);
    }

    std::vector<Polyline> candPolys;
    for (auto& cand : validCands) {
        Polyline p = rotated;
        p.translate(cand);
        candPolys.push_back(p);
    }
    std::vector<Polyline> nfpPolys;
    for (auto& g : nfps) {
        for (auto& o : g.outers) nfpPolys.push_back(o);
        for (auto& h : g.holes) nfpPolys.push_back(h);
    }

    bestSP.candidatePolys = candPolys;
    bestSP.nfpPolys = nfpPolys;

    if (useBL) {
        std::sort(validCands.begin(), validCands.end(), [](const NestPoint& a, const NestPoint& b) {
            double da = a.y * 10000 + a.x;
            double db = b.y * 10000 + b.x;
            return da < db;
        });
        int slideLimit = std::min((int)validCands.size(), 50);
        for (int i = 0; i < slideLimit; ++i) {
            NestPoint finalPos = blSlide(rotated, validCands[i], nfps, config.stockWidth, config.stockHeight);
            if (!isInsideStock(rotated, finalPos)) continue;
            if (!isOutsideAllNfps(finalPos, nfps)) continue;
            ScoredPosition sp = evaluatePosition(rotated, finalPos, rot, placed);
            if (sp.score < bestSP.score) {
                bestSP.pos = sp.pos;
                bestSP.rotation = sp.rotation;
                bestSP.score = sp.score;
            }
        }
    } else {
        for (auto& cand : validCands) {
            ScoredPosition sp = evaluatePosition(rotated, cand, rot, placed);
            if (sp.score < bestSP.score) {
                bestSP.pos = sp.pos;
                bestSP.rotation = sp.rotation;
                bestSP.score = sp.score;
            }
        }
    }
    return bestSP;
}

std::vector<NestPoint> Nester::blfFill(const Polyline& poly, const std::vector<Placement>& placed) const
{
    std::vector<NestPoint> gapCandidates;
    Box2D stockBox;
    stockBox.append({0, 0});
    stockBox.append({config.stockWidth, config.stockHeight});

    double step = std::max(5.0, std::min(config.stockWidth, config.stockHeight) * 0.02);

    for (double y = 0; y <= config.stockHeight; y += step) {
        for (double x = 0; x <= config.stockWidth; x += step) {
            NestPoint pt = {x, y};
            if (!isInsideStock(poly, pt)) continue;

            bool inGap = true;
            for (const auto& p : placed) {
                NestPoly fixed = p.polygon;
                fixed.translate(p.offset);
                ClipperLib::IntPoint cp;
                cp.X = static_cast<ClipperLib::cInt>(std::llround(pt.x * clipperScaler));
                cp.Y = static_cast<ClipperLib::cInt>(std::llround(pt.y * clipperScaler));
                ClipperLib::Path path = polygon2Path(fixed);
                if (ClipperLib::PointInPolygon(cp, path) != 0) {
                    inGap = false;
                    break;
                }
            }
            if (inGap)
                gapCandidates.push_back(pt);
        }
    }
    return gapCandidates;
}

bool Nester::backtrackPlace(std::vector<int>& order, int depth, std::vector<Placement>& result,
                            bool useBL)
{
    if (depth >= (int)order.size())
        return true;

    Polyline poly = polygons[order[depth]];
    cleanPolygon(poly);
    if (poly.size() < 3)
        return backtrackPlace(order, depth + 1, result, useBL);
    if (poly.orientation() == Polyline::Clockwise)
        poly.reverse();

    auto rotations = getRotationAngles();
    ScoredPosition bestSP;
    bestSP.score = 1e18;
    bool found = false;

    for (double rot : rotations) {
        NestPoly rotated = poly;
        if (std::fabs(rot) > 1e-9)
            rotated.rotate(rot);

        auto nfps = computeNfpsForMoving(rotated, result, config.nfpMethod);
        auto flatNfps = flattenNfpGroups(nfps);

        std::vector<NestPoint> candidates;
        candidates.push_back({0, 0});
        for (const auto& p : result) {
            NestPoly fixed = p.polygon;
            fixed.translate(p.offset);
            auto nfpC = getNfpVertices(fixed, rotated, config.nfpMethod);
            candidates.insert(candidates.end(), nfpC.begin(), nfpC.end());
        }

        Box2D bb = calcBoundingBox(rotated);
        double nfpStep = std::max(5.0, std::min(bb.width(), bb.height()) * 0.3);
        auto nfpSamples = sampleNfpBoundary(flatNfps, nfpStep);
        candidates.insert(candidates.end(), nfpSamples.begin(), nfpSamples.end());

        for (auto& cand : candidates) {
            if (!isInsideStock(rotated, cand)) continue;
            if (!isOutsideAllNfps(cand, nfps)) continue;

            ScoredPosition sp;
            if (useBL) {
                NestPoint finalPos = blSlide(rotated, cand, nfps, config.stockWidth, config.stockHeight);
                if (!isInsideStock(rotated, finalPos)) continue;
                if (!isOutsideAllNfps(finalPos, nfps)) continue;
                sp = evaluatePosition(rotated, finalPos, rot, result);
            } else {
                sp = evaluatePosition(rotated, cand, rot, result);
            }

            if (sp.score < bestSP.score) {
                bestSP = sp;
                bestSP.rotation = rot;
                found = true;
            }
        }
    }

    if (!found) {
        if (depth < config.backtrackDepth && depth > 0) {
            result.pop_back();
            return backtrackPlace(order, depth - 1, result, useBL);
        }
        return false;
    }

    Polyline placedPoly = poly;
    if (std::fabs(bestSP.rotation) > 1e-9)
        placedPoly.rotate(bestSP.rotation);
    result.push_back({placedPoly, bestSP.pos, bestSP.rotation});

    return backtrackPlace(order, depth + 1, result, useBL);
}

void Nester::simulatedAnnealing(std::vector<Placement>& currentPlacements, int iterations) const
{
    if (currentPlacements.size() < 2) return;

    double currentScore = computePlacedBBox(currentPlacements).width() *
                          computePlacedBBox(currentPlacements).height();
    double bestScore = currentScore;
    auto bestPlacements = currentPlacements;

    double temp = 1000.0;
    double cooling = 0.995;

    for (int iter = 0; iter < iterations; ++iter) {
        int i = rng() % currentPlacements.size();
        int j = rng() % currentPlacements.size();
        if (i == j) continue;

        auto trial = currentPlacements;
        std::swap(trial[i], trial[j]);

        for (auto& p : trial) {
            p.offset = {0, 0};
        }

        std::vector<Placement> rebuilt;
        bool ok = true;
        for (auto& p : trial) {
            Polyline poly = p.polygon;
            if (std::fabs(p.rotation) > 1e-9)
                poly.rotate(p.rotation);

            auto nfps = computeNfpsForMoving(poly, rebuilt, config.nfpMethod);

            std::vector<NestPoint> candidates;
            candidates.push_back({0, 0});
            for (const auto& rp : rebuilt) {
                NestPoly fixed = rp.polygon;
                fixed.translate(rp.offset);
                auto nfpC = getNfpVertices(fixed, poly, config.nfpMethod);
                candidates.insert(candidates.end(), nfpC.begin(), nfpC.end());
            }

            NestPoint bestPos = {config.stockWidth + 1, config.stockHeight + 1};
            double bestPosScore = 1e18;
            for (auto& cand : candidates) {
                if (!isInsideStock(poly, cand)) continue;
                if (!isOutsideAllNfps(cand, nfps)) continue;
                ScoredPosition sp = evaluatePosition(poly, cand, p.rotation, rebuilt);
                if (sp.score < bestPosScore) {
                    bestPosScore = sp.score;
                    bestPos = cand;
                }
            }

            if (bestPos.x > config.stockWidth || bestPos.y > config.stockHeight) {
                ok = false;
                break;
            }
            rebuilt.push_back({poly, bestPos, p.rotation});
        }

        if (!ok) continue;

        double trialScore = computePlacedBBox(rebuilt).width() *
                            computePlacedBBox(rebuilt).height();
        double delta = trialScore - currentScore;

        if (delta < 0 || std::exp(-delta / temp) > (double)(rng() % 1000) / 1000.0) {
            currentPlacements = rebuilt;
            currentScore = trialScore;
            if (currentScore < bestScore) {
                bestScore = currentScore;
                bestPlacements = currentPlacements;
            }
        }
        temp *= cooling;
    }

    currentPlacements = bestPlacements;
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

    qDebug() << "BL sorted:" << indices << "rotation steps:" << config.rotationSteps;

    for (int idx : indices) {
        Polyline poly = polygons[idx];
        cleanPolygon(poly);
        if (poly.size() < 3) continue;
        if (poly.orientation() == Polyline::Clockwise)
            poly.reverse();

        auto rotations = getRotationAngles();

        ScoredPosition bestSP;
        bestSP.score = 1e18;
        bool found = false;

        for (double rot : rotations) {
            ScoredPosition sp = evaluateRotation(poly, rot, placements, true);

            if (!sp.candidatePolys.empty()) {
                Polyline rotated = poly;
                if (std::fabs(rot) > 1e-9)
                    rotated.rotate(rot);
                notifyCandidates(sp.candidatePolys, sp.nfpPolys, rotated);
            }

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

            double util = getUtilization();
            qDebug() << "BL placed" << idx << "at" << bestSP.pos.x << bestSP.pos.y
                     << "rot" << (bestSP.rotation * 180 / M_PI)
                     << "| util:" << util << "%";
            notifyStep(idx);
        } else {
            qDebug() << "BL skipped" << idx;
        }
    }

    if (config.enableBLF) {
        qDebug() << "BLF gap filling...";
        for (int idx : indices) {
            Polyline poly = polygons[idx];
            cleanPolygon(poly);
            if (poly.size() < 3) continue;
            if (poly.orientation() == Polyline::Clockwise)
                poly.reverse();

            bool inPlaced = false;
            for (size_t pi = 0; pi < placements.size(); ++pi) {
                if (std::fabs(placements[pi].polygon.area() - poly.area()) < 1e-6 &&
                    placements[pi].polygon.size() == poly.size()) {
                    inPlaced = true;
                    break;
                }
            }
            if (inPlaced) continue;

            auto gapCands = blfFill(poly, placements);
            ScoredPosition bestGap;
            bestGap.score = 1e18;
            bool gapFound = false;

            for (auto& cand : gapCands) {
                for (double rot : {0.0, M_PI / 2, M_PI, 3 * M_PI / 2}) {
                    NestPoly rotated = poly;
                    if (std::fabs(rot) > 1e-9)
                        rotated.rotate(rot);
                    if (!isInsideStock(rotated, cand)) continue;

                    auto nfps = computeNfpsForMoving(rotated, placements, config.nfpMethod);
                    if (!isOutsideAllNfps(cand, nfps)) continue;

                    ScoredPosition sp = evaluatePosition(rotated, cand, rot, placements);
                    if (sp.score < bestGap.score) {
                        bestGap = sp;
                        gapFound = true;
                    }
                }
            }

            if (gapFound) {
                Polyline placedPoly = poly;
                if (std::fabs(bestGap.rotation) > 1e-9)
                    placedPoly.rotate(bestGap.rotation);
                placements.push_back({placedPoly, bestGap.pos, bestGap.rotation});
                qDebug() << "BLF filled" << idx << "at" << bestGap.pos.x << bestGap.pos.y;
                notifyStep(idx);
            }
        }
    }

    if (config.enableSA && placements.size() >= 2) {
        qDebug() << "Simulated annealing optimization...";
        simulatedAnnealing(placements, config.saIterations);
        qDebug() << "SA done, util:" << getUtilization() << "%";
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

    qDebug() << "Greedy sorted:" << indices << "rotation steps:" << config.rotationSteps;

    if (config.enableBacktrack) {
        std::vector<int> order = indices;
        std::vector<Placement> result;
        if (backtrackPlace(order, 0, result, false)) {
            placements = result;
            for (size_t i = 0; i < placements.size(); ++i)
                notifyStep((int)i);
        }
    } else {
        for (int idx : indices) {
            Polyline poly = polygons[idx];
            cleanPolygon(poly);
            if (poly.size() < 3) continue;
            if (poly.orientation() == Polyline::Clockwise)
                poly.reverse();

            auto rotations = getRotationAngles();

            ScoredPosition bestSP;
            bestSP.score = 1e18;
            bool found = false;

            for (double rot : rotations) {
                ScoredPosition sp = evaluateRotation(poly, rot, placements, false);

                if (!sp.candidatePolys.empty()) {
                    Polyline rotated = poly;
                    if (std::fabs(rot) > 1e-9)
                        rotated.rotate(rot);
                    notifyCandidates(sp.candidatePolys, sp.nfpPolys, rotated);
                }

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

                double util = getUtilization();
                qDebug() << "Greedy placed" << idx << "at" << bestSP.pos.x << bestSP.pos.y
                         << "rot" << (bestSP.rotation * 180 / M_PI)
                         << "| util:" << util << "%";
                notifyStep(idx);
            } else {
                qDebug() << "Greedy skipped" << idx;
            }
        }
    }

    if (config.enableSA && placements.size() >= 2) {
        qDebug() << "Simulated annealing optimization...";
        simulatedAnnealing(placements, config.saIterations);
        qDebug() << "SA done, util:" << getUtilization() << "%";
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
