#include "nfpplacer.h"

#include <QDebug>
#include "shapes/utiltool.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace S_Shape2D {

namespace {

using NfpPoint = NfpPlacer::Point;
using NfpPolyline = NfpPlacer::Polyline;
using ClipperPath = ClipperLib::Path;
using ClipperPaths = ClipperLib::Paths;

constexpr double splitEps = 1e-8;
constexpr double pointKeyScale = 1000000.0;

struct Segment {
    NfpPoint a;
    NfpPoint b;
};

using PointKey = std::pair<long long, long long>;

std::vector<NfpPolyline> extractRings(const std::vector<Segment>& segments);
std::vector<NfpPolyline> simplifyNfpRings(const std::vector<NfpPolyline>& rings);
std::vector<NfpPolyline> moveTouchingPointToRingStart(const std::vector<NfpPolyline>& rings, const NfpPolyline& fixed, const NfpPolyline& moving);
NfpPoint leftRightTouchReferencePoint(const NfpPolyline& fixed, const NfpPolyline& moving);

ClipperPath relativePath(const NfpPolyline& polygon, const NfpPoint& reference)
{
    ClipperPath path;
    path.reserve(polygon.size());
    for (const auto& pt : polygon) {
        path.push_back({
            static_cast<ClipperLib::cInt>(std::llround((pt.x - reference.x) * clipperScaler)),
            static_cast<ClipperLib::cInt>(std::llround((pt.y - reference.y) * clipperScaler))
        });
    }
    return path;
}

PointKey keyOf(const NfpPoint& pt)
{
    return {
        static_cast<long long>(std::llround(pt.x * pointKeyScale)),
        static_cast<long long>(std::llround(pt.y * pointKeyScale))
    };
}

NfpPoint pointAt(const Segment& segment, double t)
{
    return segment.a + (segment.b - segment.a) * t;
}

bool inUnit(double value)
{
    return value > -splitEps && value < 1.0 + splitEps;
}

double segmentParameter(const Segment& segment, const NfpPoint& pt)
{
    NfpPoint dir = segment.b - segment.a;
    if (std::fabs(dir.x) >= std::fabs(dir.y)) {
        if (isEqual(dir.x, 0.0))
            return 0.0;
        return (pt.x - segment.a.x) / dir.x;
    }

    if (isEqual(dir.y, 0.0))
        return 0.0;
    return (pt.y - segment.a.y) / dir.y;
}

void addParam(std::vector<double>& params, double value)
{
    if (!inUnit(value))
        return;

    value = (std::max)(0.0, (std::min)(1.0, value));
    for (double old : params) {
        if (std::fabs(old - value) < splitEps)
            return;
    }
    params.push_back(value);
}

bool pointOnSegment(const NfpPoint& pt, const Segment& segment)
{
    NfpPoint ab = segment.b - segment.a;
    NfpPoint ap = pt - segment.a;
    if (!isEqual(ab.cross(ap), 0.0))
        return false;

    double t = segmentParameter(segment, pt);
    return inUnit(t);
}

NfpPoint leftRightTouchReferencePoint(const NfpPolyline& fixed, const NfpPolyline& moving)
{
    if (fixed.empty() || moving.empty())
        return {};

    size_t minIndexA = 0;
    size_t maxIndexB = 0;
    for (size_t i = 1; i < fixed.size(); ++i) {
        if ((isEqual(fixed[minIndexA].x, fixed[i].x) && fixed[minIndexA].y > fixed[i].y)
            || fixed[minIndexA].x > fixed[i].x) {
            minIndexA = i;
        }
    }

    for (size_t i = 1; i < moving.size(); ++i) {
        if ((isEqual(moving[maxIndexB].x, moving[i].x) && moving[maxIndexB].y < moving[i].y)
            || moving[maxIndexB].x < moving[i].x) {
            maxIndexB = i;
        }
    }

    return fixed[minIndexA] - (moving[maxIndexB] - moving[0]);
}

double signedLineDistance(const NfpPoint& lineStart, const NfpPoint& lineDir, const NfpPoint& pt)
{
    return lineDir.cross(pt - lineStart);
}

bool polygonOnRightSideOfLine(const NfpPolyline& polygon, const NfpPoint& lineStart, const NfpPoint& lineDir)
{
    const double tolerance = (std::max)(1e-6, lineDir.norm() * 1e-8);
    for (const auto& pt : polygon) {
        if (signedLineDistance(lineStart, lineDir, pt) > tolerance)
            return false;
    }
    return true;
}

double pointSegmentDistance(const NfpPoint& pt, const NfpPoint& a, const NfpPoint& b)
{
    NfpPoint ab = b - a;
    double len2 = ab.square();
    if (isEqual(len2, 0.0))
        return pt.distance(a);

    double t = (pt - a).dot(ab) / len2;
    t = (std::max)(0.0, (std::min)(1.0, t));
    NfpPoint projected = a + ab * t;
    return pt.distance(projected);
}

double polygonDistance(const NfpPolyline& fixed, const NfpPolyline& moving, const NfpPoint& movingReference, const NfpPoint& referencePosition)
{
    NfpPolyline placed = moving;
    placed.translate(referencePosition - movingReference);

    double best = std::numeric_limits<double>::max();
    for (size_t i = 0; i < fixed.size(); ++i) {
        const NfpPoint& a0 = fixed[i];
        const NfpPoint& a1 = fixed[(i + 1) % fixed.size()];
        for (const auto& pt : placed)
            best = (std::min)(best, pointSegmentDistance(pt, a0, a1));
    }

    for (size_t i = 0; i < placed.size(); ++i) {
        const NfpPoint& b0 = placed[i];
        const NfpPoint& b1 = placed[(i + 1) % placed.size()];
        for (const auto& pt : fixed)
            best = (std::min)(best, pointSegmentDistance(pt, b0, b1));
    }

    return best;
}

std::vector<Segment> splitSegments(const std::vector<Segment>& segments)
{
    std::vector<std::vector<double>> params(segments.size());
    for (auto& p : params) {
        p.push_back(0.0);
        p.push_back(1.0);
    }

    for (size_t i = 0; i < segments.size(); ++i) {
        NfpPoint r = segments[i].b - segments[i].a;
        if (isEqual(r.norm(), 0.0))
            continue;

        for (size_t j = i + 1; j < segments.size(); ++j) {
            NfpPoint s = segments[j].b - segments[j].a;
            if (isEqual(s.norm(), 0.0))
                continue;

            NfpPoint qp = segments[j].a - segments[i].a;
            double denom = r.cross(s);
            if (!isEqual(denom, 0.0)) {
                double t = qp.cross(s) / denom;
                double u = qp.cross(r) / denom;
                if (inUnit(t) && inUnit(u)) {
                    addParam(params[i], t);
                    addParam(params[j], u);
                }
                continue;
            }

            if (!isEqual(qp.cross(r), 0.0))
                continue;

            if (pointOnSegment(segments[j].a, segments[i]))
                addParam(params[i], segmentParameter(segments[i], segments[j].a));
            if (pointOnSegment(segments[j].b, segments[i]))
                addParam(params[i], segmentParameter(segments[i], segments[j].b));
            if (pointOnSegment(segments[i].a, segments[j]))
                addParam(params[j], segmentParameter(segments[j], segments[i].a));
            if (pointOnSegment(segments[i].b, segments[j]))
                addParam(params[j], segmentParameter(segments[j], segments[i].b));
        }
    }

    std::vector<Segment> result;
    for (size_t i = 0; i < segments.size(); ++i) {
        std::sort(params[i].begin(), params[i].end());
        for (size_t j = 1; j < params[i].size(); ++j) {
            if (params[i][j] - params[i][j - 1] < splitEps)
                continue;

            Segment segment { pointAt(segments[i], params[i][j - 1]), pointAt(segments[i], params[i][j]) };
            if (!isEqual((segment.b - segment.a).norm(), 0.0))
                result.push_back(segment);
        }
    }

    return result;
}

double intersectionArea(const NfpPolyline& fixed, const NfpPolyline& moving, const NfpPoint& movingReference, const NfpPoint& referencePosition)
{
    NfpPolyline placed = moving;
    placed.translate(referencePosition - movingReference);

    ClipperLib::Paths overlap = Intersection(polygon2Path(fixed), polygon2Path(placed));
    double area = 0.0;
    for (const auto& path : overlap) {
        area += std::fabs(ClipperLib::Area(path)) / (clipperScaler * static_cast<double>(clipperScaler));
    }
    return area;
}

std::vector<Segment> ringToSegments(const NfpPolyline& ring)
{
    std::vector<Segment> segments;
    if (ring.size() < 2)
        return segments;

    segments.reserve(ring.size());
    for (size_t i = 0; i < ring.size(); ++i) {
        Segment segment { ring[i], ring[(i + 1) % ring.size()] };
        if (!isEqual((segment.b - segment.a).norm(), 0.0))
            segments.push_back(segment);
    }
    return segments;
}

std::vector<NfpPolyline> sanitizeNfpRings(const std::vector<NfpPolyline>& rings, const NfpPolyline& fixed, const NfpPolyline& moving)
{
    std::vector<Segment> validEdges;
    if (fixed.size() < 3 || moving.size() < 3)
        return {};

    const NfpPoint reference = moving[0];
    const double areaTolerance = 1e-5;

    for (const auto& ring : rings) {
        for (const auto& edge : ringToSegments(ring)) {
            NfpPoint mid = edge.a + (edge.b - edge.a) * 0.5;
            if (intersectionArea(fixed, moving, reference, mid) <= areaTolerance)
                validEdges.push_back(edge);
        }
    }

    std::vector<NfpPolyline> sanitized = extractRings(validEdges);
    if (!sanitized.empty())
        return simplifyNfpRings(sanitized);

    return simplifyNfpRings(rings);
}

std::vector<Segment> generateCandidateSegments(const NfpPolyline& fixed, const NfpPolyline& moving)
{
    std::vector<Segment> segments;
    if (fixed.empty() || moving.empty())
        return segments;

    const NfpPoint reference = moving[0];
    for (size_t i = 0; i < fixed.size(); ++i) {
        const NfpPoint& a0 = fixed[i];
        const NfpPoint& a1 = fixed[(i + 1) % fixed.size()];
        NfpPoint edge = a1 - a0;
        for (size_t j = 0; j < moving.size(); ++j) {
            if (!polygonOnRightSideOfLine(moving, moving[j], edge))
                continue;

            NfpPoint rel = moving[j] - reference;
            segments.push_back({ a0 - rel, a1 - rel });
        }
    }

    for (size_t i = 0; i < fixed.size(); ++i) {
        const NfpPoint& vertex = fixed[i];
        for (size_t j = 0; j < moving.size(); ++j) {
            const NfpPoint& b0 = moving[j];
            const NfpPoint& b1 = moving[(j + 1) % moving.size()];
            NfpPoint edge = b1 - b0;
            if (!polygonOnRightSideOfLine(fixed, vertex, edge))
                continue;

            segments.push_back({ vertex - (b0 - reference), vertex - (b1 - reference) });
        }
    }

    return segments;
}

std::vector<Segment> filterBoundarySegments(const std::vector<Segment>& segments, const NfpPolyline& fixed, const NfpPolyline& moving)
{
    std::vector<Segment> result;
    if (moving.empty())
        return result;

    Box2D box = calcBoundingBox(fixed) | calcBoundingBox(moving);
    double probe = ((std::max)(box.width(), box.height()) * 1e-6);
    if (probe < 1e-4)
        probe = 1e-4;

    const NfpPoint reference = moving[0];
    const double areaEps = probe * probe;

    std::set<std::pair<PointKey, PointKey>> seen;
    for (const auto& segment : segments) {
        NfpPoint dir = segment.b - segment.a;
        double length = dir.norm();
        if (isEqual(length, 0.0))
            continue;

        NfpPoint unit = dir / length;
        NfpPoint left(-unit.y, unit.x);
        NfpPoint mid = segment.a + dir * 0.5;
        bool leftOverlaps = intersectionArea(fixed, moving, reference, mid + left * probe) > areaEps;
        bool rightOverlaps = intersectionArea(fixed, moving, reference, mid - left * probe) > areaEps;

        if (leftOverlaps == rightOverlaps)
            continue;

        Segment oriented = leftOverlaps ? segment : Segment { segment.b, segment.a };
        auto k = std::make_pair(keyOf(oriented.a), keyOf(oriented.b));
        if (seen.insert(k).second)
            result.push_back(oriented);
    }

    return result;
}

double turnAngle(const NfpPoint& from, const NfpPoint& to)
{
    return std::atan2(from.cross(to), from.dot(to));
}

std::vector<NfpPolyline> extractRings(const std::vector<Segment>& segments)
{
    std::map<PointKey, std::vector<size_t>> outgoing;
    for (size_t i = 0; i < segments.size(); ++i) {
        outgoing[keyOf(segments[i].a)].push_back(i);
    }

    std::vector<bool> used(segments.size(), false);
    std::vector<NfpPolyline> rings;

    for (size_t startIndex = 0; startIndex < segments.size(); ++startIndex) {
        if (used[startIndex])
            continue;

        NfpPolyline ring;
        size_t currentIndex = startIndex;
        PointKey startKey = keyOf(segments[startIndex].a);
        PointKey currentKey = startKey;
        NfpPoint prevDir = segments[startIndex].b - segments[startIndex].a;
        std::vector<size_t> path;
        std::set<size_t> localUsed;
        bool closed = false;

        for (size_t guard = 0; guard < segments.size() + 1; ++guard) {
            const Segment& current = segments[currentIndex];
            path.push_back(currentIndex);
            localUsed.insert(currentIndex);

            if (ring.empty() || ring[ring.size() - 1] != current.a)
                ring.add(current.a);

            currentKey = keyOf(current.b);
            if (currentKey == startKey) {
                if (ring.size() >= 3) {
                    rings.push_back(ring);
                    for (size_t index : path)
                        used[index] = true;
                    closed = true;
                }
                break;
            }

            auto it = outgoing.find(currentKey);
            if (it == outgoing.end())
                break;

            size_t nextIndex = static_cast<size_t>(-1);
            double bestAngle = std::numeric_limits<double>::max();
            for (size_t candidate : it->second) {
                if (used[candidate] || localUsed.find(candidate) != localUsed.end())
                    continue;

                NfpPoint candidateDir = segments[candidate].b - segments[candidate].a;
                double angle = turnAngle(prevDir, candidateDir);
                if (angle < bestAngle) {
                    bestAngle = angle;
                    nextIndex = candidate;
                }
            }

            if (nextIndex == static_cast<size_t>(-1))
                break;

            prevDir = segments[nextIndex].b - segments[nextIndex].a;
            currentIndex = nextIndex;
        }

        if (!closed && path.size() == 1)
            used[startIndex] = true;
    }

    std::sort(rings.begin(), rings.end(), [](NfpPolyline lhs, NfpPolyline rhs) {
        return std::fabs(lhs.area()) > std::fabs(rhs.area());
    });
    return rings;
}

std::vector<NfpPolyline> minkowskiBoundaryFallback(const NfpPolyline& fixed, const NfpPolyline& moving)
{
    std::vector<NfpPolyline> rings;
    if (fixed.size() < 3 || moving.size() < 3)
        return rings;

    ClipperPaths solution;
    ClipperLib::MinkowskiDiff(relativePath(moving, moving[0]), polygon2Path(fixed), solution);
    ClipperLib::CleanPolygons(solution, 1.0);

    std::sort(solution.begin(), solution.end(), [](const ClipperPath& lhs, const ClipperPath& rhs) {
        return std::fabs(ClipperLib::Area(lhs)) > std::fabs(ClipperLib::Area(rhs));
    });

    for (const auto& path : solution) {
        if (path.size() < 3 || isEqual(std::fabs(ClipperLib::Area(path)), 0.0))
            continue;

        NfpPolyline poly = path2Polygon<double>(path);
        cleanPolygon(poly);
        if (poly.size() >= 3)
            rings.push_back(poly);
    }

    return rings;
}

std::vector<NfpPolyline> simplifyNfpRings(const std::vector<NfpPolyline>& rings)
{
    ClipperPaths input;
    for (const auto& ring : rings) {
        if (ring.size() < 3)
            continue;

        ClipperPath path = polygon2Path(ring);
        if (path.size() >= 3)
            input.push_back(path);
    }

    if (input.empty())
        return {};

    ClipperLib::CleanPolygons(input, 1.0);

    ClipperPaths simplified;
    ClipperLib::SimplifyPolygons(input, simplified, ClipperLib::PolyFillType::pftNonZero);
    ClipperLib::CleanPolygons(simplified, 1.0);

    std::vector<NfpPolyline> result;
    for (const auto& path : simplified) {
        if (path.size() < 3 || isEqual(std::fabs(ClipperLib::Area(path)), 0.0))
            continue;

        NfpPolyline poly = path2Polygon<double>(path);
        cleanPolygon(poly);
        if (poly.size() >= 3)
            result.push_back(poly);
    }

    std::sort(result.begin(), result.end(), [](NfpPolyline lhs, NfpPolyline rhs) {
        return std::fabs(lhs.area()) > std::fabs(rhs.area());
    });

    return result.empty() ? rings : result;
}

std::vector<NfpPolyline> moveTouchingPointToRingStart(const std::vector<NfpPolyline>& rings, const NfpPolyline& fixed, const NfpPolyline& moving)
{
    if (fixed.size() < 3 || moving.size() < 3)
        return rings;

    std::vector<NfpPolyline> result = rings;
    const NfpPoint reference = moving[0];
    const NfpPoint guaranteedTouch = leftRightTouchReferencePoint(fixed, moving);
    const double overlapTolerance = 1e-5;

    bool firstRing = true;
    for (auto& ring : result) {
        if (ring.size() < 2)
            continue;

        if (firstRing && guaranteedTouch) {
            NfpPolyline rotated;
            rotated.add(guaranteedTouch);

            size_t closestIndex = 0;
            double closestDistance = std::numeric_limits<double>::max();
            for (size_t i = 0; i < ring.size(); ++i) {
                double distance = guaranteedTouch.distance(ring[i]);
                if (distance < closestDistance) {
                    closestDistance = distance;
                    closestIndex = i;
                }
            }

            for (size_t i = 0; i < ring.size(); ++i) {
                const NfpPoint& pt = ring[(closestIndex + i) % ring.size()];
                if (pt != guaranteedTouch)
                    rotated.add(pt);
            }

            ring = rotated;
            firstRing = false;
            continue;
        }

        size_t bestIndex = 0;
        double bestDistance = std::numeric_limits<double>::max();
        for (size_t i = 0; i < ring.size(); ++i) {
            double overlap = intersectionArea(fixed, moving, reference, ring[i]);
            double distance = polygonDistance(fixed, moving, reference, ring[i]);
            if (overlap > overlapTolerance)
                distance += overlap * 1000.0;

            if (distance < bestDistance) {
                bestDistance = distance;
                bestIndex = i;
            }
        }

        if (bestIndex == 0)
            continue;

        NfpPolyline rotated;
        for (size_t i = 0; i < ring.size(); ++i)
            rotated.add(ring[(bestIndex + i) % ring.size()]);
        ring = rotated;
    }

    return result;
}

}

NfpPlacer::NfpPlacer(NfpPlacer::CPolylineRef p1, NfpPlacer::CPolylineRef p2)
    : polyA(p1)
    , polyB(p2)
{
}

void NfpPlacer::set(NfpPlacer::CPolylineRef A, NfpPlacer::CPolylineRef B)
{
    polyA = A;
    polyB = B;

    nfps.clear();
    isExecute = false;
}

void NfpPlacer::exec()
{
    if (isExecute)
        return;
    nfps.clear();

    if (polyA.size() < 3 || polyB.size() < 3) {
        isExecute = true;
        return;
    }

    Polyline fixedForCheck = polyA;
    Polyline movingForCheck = polyB;

    size_t minIndexA = 0;
    size_t maxIndexB = 0;

    // Start with A's leftmost vertex touching B's rightmost vertex.
    Coord minX = polyA[0].x;
    Coord minY = polyA[0].y;
    Coord maxX = polyB[0].x;
    Coord maxY = polyB[0].y;
    for (size_t i = 0; i < polyA.size(); ++i) {

        if ((isEqual(minX, polyA[i].x) && minY > polyA[i].y)
            || minX > polyA[i].x) {
            minY = polyA[i].y;
            minX = polyA[i].x;
            minIndexA = i;
        }
    }

    //找到polyB x最大的点
    for (size_t i = 0; i < polyB.size(); ++i) {

        if ((isEqual(maxX, polyB[i].x) && maxY < polyB[i].y)
            || maxX < polyB[i].x) {
            maxY = polyB[i].y;
            maxX = polyB[i].x;
            maxIndexB = i;
        }
    }

    Polyline nfp;

    //起始状态， A x最小点和B x最大点相接触
    Touch start { minIndexA, maxIndexB, POINT_POINT };
    Touch curTouch = start;

    Point dir;
    Point ptBase = polyA[minIndexA];
    nfp.add(ptBase);

    //B 移动至起始状态
    polyB.translate(ptBase - polyB[maxIndexB]);

    Coord dis;

    //循环求解
    do {

        //获取nfp移动方向
        dir = getDir(curTouch);

        //获取移动距离
        Coord norm = dir.norm();
        if (isEqual(Coord(0), norm)) {
            break;
        }
        dis = getDistance(dir / norm);

        //如果dis为0，直接退出，认为当前走向无解
        //选取min(dis, norm)作为实际移动距离
        if (isEqual(Coord(0), dis)) {
            break;
        } else if (norm <= dis) {
            ptBase.translate(dir);
            //移动polyB
        } else {
            //加入
            auto d = (dir / norm) * dis;
            ptBase.translate(d);
        }

        //矫正nfp点，如果nfp点为多边行顶点，直接矫正为顶点，一定程度上防止偏移
        for (const Point& pt : polyA) {
            if (pt == ptBase) {
                ptBase = pt;
                break;
            }
        }

        //移动polyB
        polyB.translate(ptBase - polyB[maxIndexB]);

        //如果找到相同的点，直接退出。（可能会造成问题）
//        bool quit = false;
//        for (const Point& pt : nfp) {
//            if (pt == ptBase) {
//                quit = true;
//                break;
//            }
//        }

        //加入nfp
        nfp.add(ptBase);

//        if (quit) {
//            break;
//        }

//                qDebug() << "NFP" << ptBase.x << ptBase.y << dis << norm;
        //查找下一个接触位置
        curTouch = getNextTouch(curTouch);

        //        qDebug() << (curTouch != Touch()) << (ptBase != polyA[minIndexA]);
    } while (/*curTouch != start &&*/ curTouch != Touch() && ptBase != polyA[minIndexA]);
    //接触位置回到起始接触位置，或者找不到下一个接触位，或者当前nfp点回到起点，循环结束

    auto offset = (polyB[0] - polyB[maxIndexB]);
    for (Point& pt : nfp) {
        pt += offset;
    }

    nfps.push_back(nfp);
    nfps = sanitizeNfpRings(nfps, fixedForCheck, movingForCheck);
    nfps = simplifyNfpRings(nfps);
    nfps = moveTouchingPointToRingStart(nfps, fixedForCheck, movingForCheck);

    isExecute = true;
}

void NfpPlacer::execVectorSegments()
{
    if (isExecute)
        return;
    nfps.clear();

    if (polyA.size() < 3 || polyB.size() < 3) {
        isExecute = true;
        return;
    }

    Polyline fixed = polyA;
    Polyline moving = polyB;
    cleanPolygon(fixed);
    cleanPolygon(moving);

    if (fixed.size() < 3 || moving.size() < 3) {
        isExecute = true;
        return;
    }

    if (Polyline::Clockwise == fixed.orientation())
        fixed.reverse();
    if (Polyline::Clockwise == moving.orientation())
        moving.reverse();

    std::vector<Segment> candidates = generateCandidateSegments(fixed, moving);
    std::vector<Segment> split = splitSegments(candidates);
    std::vector<Segment> boundary = filterBoundarySegments(split, fixed, moving);
    nfps = extractRings(boundary);
    if (nfps.empty()) {
        qDebug() << "Vector segment extraction produced no closed NFP ring; using direct Minkowski boundary fallback.";
        nfps = minkowskiBoundaryFallback(fixed, moving);
    }
    nfps = sanitizeNfpRings(nfps, fixed, moving);
    nfps = simplifyNfpRings(nfps);
    nfps = moveTouchingPointToRingStart(nfps, fixed, moving);

    isExecute = true;
}

NfpPlacer::Container NfpPlacer::getNFPs()
{
    return nfps;
}

NfpPlacer::Touch NfpPlacer::getNextTouch(const NfpPlacer::Touch& curTouch)
{
    (void)curTouch;
    //    qDebug() << "-------------------------------------";
    TouchVec res;

    size_t aSize = polyA.size();
    size_t bSize = polyB.size();

    size_t iApre = aSize - 1;
    size_t iBpre = bSize - 1;

    for (size_t iA = 0; iA < aSize; iApre = iA++) {

        iBpre = bSize - 1;

        for (size_t iB = 0; iB < bSize; iBpre = iB++) {

            int r = onSegment(polyB[iB], polyA[iApre], polyA[iA]);
            //            qDebug() << "A:" << iB << iApre << iA << r;
            if (r == 1) {
                res.push_back({ iApre, iB, POINT_POINT });
            } else if (r == -1) {
                res.push_back({ iApre, iB, EDGE_POINT });
            }

            r = onSegment(polyA[iA], polyB[iBpre], polyB[iB]);
            //            qDebug() << "B:" << iA << iBpre << iB << r;
            if (r == -1) {
                res.push_back({ iA, iBpre, POINT_EDGE });
            }
        }
    }

    Point curDir = getDir(curTouch);
    Coord curNorm = curDir.norm();
    bool hasCurDir = !isEqual(Coord(0), curNorm);
    if (hasCurDir)
        curDir /= curNorm;

    double bestAngle = std::numeric_limits<double>::max();
    Coord bestDistance = -1;

    //    qDebug() << "Size" << res.size();
    Touch rTouch {};
    for (Touch& t : res) {
        if (t == curTouch)
            continue;

        Point dirTmp = getDir(t);
        //        qDebug() << "Touch" << t.indexA << t.indexB << t.state << dirTmp.x << dirTmp.y;
        Coord normTmp = dirTmp.norm();
        if (isEqual(Coord(0), normTmp)) {
            continue;
        }
        Point unitDir = dirTmp / normTmp;
        Coord disTmp = getDistance(unitDir);
        //        qDebug() << disTmp << normTmp << t.indexA << t.indexB << dirTmp.x << dirTmp.y;
        disTmp = std::min(disTmp, normTmp);
        if (isEqual(Coord(0), disTmp)) {
            continue;
        }

        double angle = 0.0;
        if (hasCurDir) {
            angle = std::atan2(curDir.cross(unitDir), curDir.dot(unitDir));
            if (angle <= splitEps)
                angle += 2.0 * pi;
        }

        if (!hasCurDir || angle < bestAngle || (isEqual(angle, bestAngle) && disTmp > bestDistance)) {
            rTouch = t;
            bestAngle = angle;
            bestDistance = disTmp;
        }
    }
    //    qDebug() << "-------------------------------------";
    return rTouch;
}


NfpPlacer::Point NfpPlacer::getDir(const NfpPlacer::Touch& curTouch)
{
    //        int indexAPre = (curTouch.indexA + polyA.size() - 1) % polyA.size();
    int indexANext = (curTouch.indexA + 1) % polyA.size();
    //        int indexBPre = (curTouch.indexB + polyB.size() - 1) % polyB.size();
    int indexBNext = (curTouch.indexB + 1) % polyB.size();
    //点点相交
    if (POINT_POINT == curTouch.state) {
        Point dir1 = polyA[indexANext] - polyA[curTouch.indexA];
        Point dir2 = polyB[indexBNext] - polyB[curTouch.indexB];
        if (dir2.cross(dir1) > Coord(0)) {
            return dir1;
        } else {
            return (-dir2);
        }
    } else if (POINT_EDGE == curTouch.state) { //A的点和B的边接触，B沿自身边反方向移动
        return (polyB[curTouch.indexB] - polyB[indexBNext]);
    } else if (EDGE_POINT == curTouch.state) { //A的边和B的点接触，B沿A边方向移动
        return (polyA[indexANext] - polyA[curTouch.indexA]);
    } else {
        qDebug() << "ERROR";
    }
    return {};
}

NfpPlacer::Coord NfpPlacer::getDistance(NfpPlacer::CPointRef dir)
{
    size_t ipre = polyA.size() - 1;
    Coord minDis = INT_MAX;
    Coord curDis = 0;

    //双重循环求解多边形A沿着dir方向到多边形B的最小距离
    for (size_t i = 0; i < polyA.size(); ipre = i++) {
        size_t jpre = polyB.size() - 1;
        for (size_t j = 0; j < polyB.size(); jpre = j++) {

            //A点到B边的距离
            curDis = getProjectDis(polyA[i], polyB[jpre], polyB[j], -dir);
            //            qDebug() << "A" << i << jpre << j << curDis;
            if (curDis < minDis)
                minDis = curDis;

            //B点到A边的距离
            curDis = getProjectDis(polyB[j], polyA[ipre], polyA[i], dir);
            if (curDis < minDis)
                minDis = curDis;
        }
    }

    return minDis;
}

NfpPlacer::Coord NfpPlacer::getProjectDis(NfpPlacer::CPointRef pt, NfpPlacer::CPointRef p1, NfpPlacer::CPointRef p2, NfpPlacer::CPointRef dir)
{
    Point edge = p2 - p1;
    Coord denom = dir.cross(edge);
    if (onSegment(pt, p1, p2) == -1) {
        if (denom < Coord(-1e-7))
            return 0;
        return INT_MAX;
    }

    if (isEqual(denom, Coord(0)))
        return INT_MAX;

    Point delta = p1 - pt;
    Coord t = delta.cross(edge) / denom;
    Coord u = delta.cross(dir) / denom;

    if (t <= Coord(1e-7) || u < Coord(-1e-7) || u > Coord(1) + Coord(1e-7))
        return INT_MAX;

    return t;
}

int NfpPlacer::onSegment(NfpPlacer::CPointRef pt, NfpPlacer::CPointRef p1, NfpPlacer::CPointRef p2)
{
    if (pt == p1) {
        return 1;
    }

    //p2点不做判断
    if (pt == p2) {
        return 0;
    }

    //    qDebug() << isEqual(Coord(0), (pt - p1).cross(pt - p2)) << (pt - p1).cross(pt - p2);
    //    qDebug() << (pt - p1).square() + (pt - p2).square() << (p1 - p2).square();

    Coord dis0 = (pt - p1).square() + (pt - p2).square();
    Coord dis1 = (p1 - p2).square();
    if (isEqual(Coord(0), (pt - p1).cross(pt - p2))
            && (isEqual(dis0, dis1) || dis0 < dis1)) {
        return -1;
    }

    return 0;
}

NfpPlacer::Point NfpPlacer::intersect(NfpPlacer::CPointRef p0, NfpPlacer::CPointRef p1, NfpPlacer::CPointRef p2, NfpPlacer::CPointRef p3)
{
    Coord d01 = (p1 - p0).cross(p3 - p2);
    Coord d02 = (p2 - p0).cross(p3 - p2);
    Coord d021 = (p0 - p2).cross(p1 - p0);

    if (isEqual(Coord(0), d01)) { //平行
        return {};
    }
    // pt = p0 + k* (p1-p0)
    Coord k = d02 / d01;
    // pt = p2 + u* (p3-p2)
    Coord u = -d021 / d01;

    if ((isEqual(Coord(0), k) || k > 0) && u < 1 && u > 0) {
        return p2 + (p3 - p2) * u;
    }

    return {};
}
}
