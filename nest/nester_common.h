#ifndef NESTER_COMMON_H
#define NESTER_COMMON_H

#include "nfpplacer.h"
#include "shapes/s_box.hpp"
#include "shapes/utiltool.h"
#include <cmath>
#include <vector>

namespace S_Shape2D {

using NestPoly = S_Polyline2D;
using NestPoint = S_Point2D;

static constexpr double kNestEps = 1e-6;

struct NfpGroup {
    std::vector<NestPoly> outers;
    std::vector<NestPoly> holes;
};

inline double signedArea2x(const NestPoly& poly)
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

inline bool ptInside(const NestPoint& pt, const NestPoly& poly)
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

inline bool segsCross(const NestPoint& a0, const NestPoint& a1,
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

inline bool pointStrictlyInside(const NestPoint& pt, const ClipperLib::Path& path)
{
    ClipperLib::IntPoint cp;
    cp.X = static_cast<ClipperLib::cInt>(std::llround(pt.x * clipperScaler));
    cp.Y = static_cast<ClipperLib::cInt>(std::llround(pt.y * clipperScaler));
    return ClipperLib::PointInPolygon(cp, path) == 1;
}

inline bool pointStrictlyInside(const NestPoint& pt, const NestPoly& poly)
{
    return pointStrictlyInside(pt, polygon2Path(poly));
}

inline bool polysReallyOverlap(const NestPoly& a, const NestPoly& b)
{
    size_t na = a.size();
    size_t nb = b.size();
    ClipperLib::Path pathA = polygon2Path(a);
    ClipperLib::Path pathB = polygon2Path(b);

    for (size_t i = 0; i < na; ++i) {
        NestPoint a0 = a[i];
        NestPoint a1 = a[(i + 1) % na];
        for (size_t j = 0; j < nb; ++j) {
            if (segsCross(a0, a1, b[j], b[(j + 1) % nb]))
                return true;
        }
    }
    for (size_t i = 0; i < na; ++i) {
        if (pointStrictlyInside(a[i], pathB)) return true;
    }
    for (size_t j = 0; j < nb; ++j) {
        if (pointStrictlyInside(b[j], pathA)) return true;
    }
    return false;
}

inline Box2D translatedBoundingBox(const NestPoly& poly, const NestPoint& offset)
{
    Box2D box;
    for (const auto& pt : poly)
        box.append(pt + offset);
    return box;
}

inline bool overlapsAnyPlaced(const NestPoly& poly, const std::vector<Nester::Placement>& placed)
{
    Box2D polyBox = calcBoundingBox(poly);
    for (const auto& p : placed) {
        Box2D placedBox = translatedBoundingBox(p.polygon, p.offset);
        if (polyBox.right() < placedBox.left() || polyBox.left() > placedBox.right() ||
            polyBox.bottom() < placedBox.top() || polyBox.top() > placedBox.bottom())
            continue;
        NestPoly fixed = p.polygon;
        fixed.translate(p.offset);
        if (polysReallyOverlap(fixed, poly))
            return true;
    }
    return false;
}

inline std::vector<NfpGroup> computeNfpsForMoving(const NestPoly& moving,
                                                   const std::vector<Nester::Placement>& placed,
                                                   int method)
{
    std::vector<NfpGroup> result;
    NestPoint refOffset = moving[0];

    for (const auto& p : placed) {
        NestPoly fixed = p.polygon;
        fixed.translate(p.offset);
        NfpPlacer placer(fixed, moving);
        switch (method) {
        case 0:
            placer.exec();
            break;
        case 1:
            placer.execVectorSegments();
            break;
        default:
            placer.execMinkowski();
            break;
        }
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

inline bool isOutsideAllNfps(const NestPoint& pt, const std::vector<NfpGroup>& nfpGroups)
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

inline std::vector<NestPoly> flattenNfpGroups(const std::vector<NfpGroup>& groups)
{
    std::vector<NestPoly> result;
    for (const auto& g : groups) {
        for (const auto& o : g.outers) result.push_back(o);
        for (const auto& h : g.holes) result.push_back(h);
    }
    return result;
}

inline NestPoint blSlide(const NestPoly& poly, NestPoint pos,
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
                if (cand.x < -kNestEps || cand.y < -kNestEps) continue;
                if (cand.x > stockW + kNestEps || cand.y > stockH + kNestEps) continue;
                if (!isOutsideAllNfps(cand, nfpGroups)) continue;
                if (cand.y < best.y - kNestEps || (std::fabs(cand.y - best.y) < kNestEps && cand.x < best.x)) {
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

} // namespace S_Shape2D

#endif // NESTER_COMMON_H
