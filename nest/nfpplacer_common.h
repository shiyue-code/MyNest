#ifndef NFPPLACER_COMMON_H
#define NFPPLACER_COMMON_H

#include "nfpplacer.h"
#include "shapes/utiltool.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>

namespace S_Shape2D {
namespace NfpDetail {

using NfpPoint = NfpPlacer::Point;
using NfpPolyline = NfpPlacer::Polyline;
using ClipperPath = ClipperLib::Path;
using ClipperPaths = ClipperLib::Paths;

constexpr double splitEps = 1e-8;
constexpr double pointKeyScale = 10000.0;

enum class ExtremeVertex {
    LeftLowest,
    RightHighest
};

struct Segment {
    NfpPoint a;
    NfpPoint b;
};

struct SegmentBox {
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
};

struct CollisionContext {
    ClipperPath fixedPath;
    ClipperPath movingRelativePath;
};

using PointKey = std::pair<long long, long long>;

inline std::vector<NfpPolyline> extractRings(const std::vector<Segment>& segments);
inline std::vector<NfpPolyline> sanitizeNfpRings(const std::vector<NfpPolyline>& rings, const NfpPolyline& fixed, const NfpPolyline& moving);
inline std::vector<NfpPolyline> simplifyNfpRings(const std::vector<NfpPolyline>& rings);
inline std::vector<NfpPolyline> moveTouchingPointToRingStart(const std::vector<NfpPolyline>& rings,
                                                             const NfpPolyline& fixed,
                                                             const NfpPolyline& moving,
                                                             bool validateInnerStarts = true);
inline NfpPoint leftRightTouchReferencePoint(const NfpPolyline& fixed, const NfpPolyline& moving);

template <ExtremeVertex Mode>
size_t extremeVertexIndex(const NfpPolyline& polygon)
{
    static_assert(Mode == ExtremeVertex::LeftLowest || Mode == ExtremeVertex::RightHighest);
    if (polygon.empty())
        return static_cast<size_t>(-1);

    size_t best = 0;
    for (size_t i = 1; i < polygon.size(); ++i) {
        if constexpr (Mode == ExtremeVertex::LeftLowest) {
            if ((isEqual(polygon[best].x, polygon[i].x) && polygon[best].y > polygon[i].y)
                || polygon[best].x > polygon[i].x) {
                best = i;
            }
        } else {
            if ((isEqual(polygon[best].x, polygon[i].x) && polygon[best].y < polygon[i].y)
                || polygon[best].x < polygon[i].x) {
                best = i;
            }
        }
    }

    return best;
}

inline bool normalizeNfpInputs(const NfpPolyline& sourceFixed, const NfpPolyline& sourceMoving, NfpPolyline& fixed, NfpPolyline& moving)
{
    if (sourceFixed.size() < 3 || sourceMoving.size() < 3)
        return false;

    fixed = sourceFixed;
    moving = sourceMoving;
    cleanPolygon(fixed);
    cleanPolygon(moving);

    if (fixed.size() < 3 || moving.size() < 3)
        return false;

    if (NfpPolyline::Clockwise == fixed.orientation())
        fixed.reverse();
    if (NfpPolyline::Clockwise == moving.orientation())
        moving.reverse();

    return true;
}

inline void finishNfpRings(std::vector<NfpPolyline>& rings,
                           const NfpPolyline& fixed,
                           const NfpPolyline& moving,
                           bool sanitize,
                           bool validateInnerStarts = true)
{
    if (sanitize)
        rings = sanitizeNfpRings(rings, fixed, moving);

    rings = simplifyNfpRings(rings);
    rings = moveTouchingPointToRingStart(rings, fixed, moving, validateInnerStarts);
}

inline ClipperPath relativePath(const NfpPolyline& polygon, const NfpPoint& reference)
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

inline CollisionContext makeCollisionContext(const NfpPolyline& fixed, const NfpPolyline& moving, const NfpPoint& movingReference)
{
    return { polygon2Path(fixed), relativePath(moving, movingReference) };
}

inline ClipperPath translatedPath(const ClipperPath& relative, const NfpPoint& referencePosition)
{
    const auto dx = static_cast<ClipperLib::cInt>(std::llround(referencePosition.x * clipperScaler));
    const auto dy = static_cast<ClipperLib::cInt>(std::llround(referencePosition.y * clipperScaler));

    ClipperPath path;
    path.reserve(relative.size());
    for (const auto& pt : relative)
        path.push_back({ pt.X + dx, pt.Y + dy });
    return path;
}

inline PointKey keyOf(const NfpPoint& pt)
{
    return {
        static_cast<long long>(std::llround(pt.x * pointKeyScale)),
        static_cast<long long>(std::llround(pt.y * pointKeyScale))
    };
}

inline NfpPoint pointAt(const Segment& segment, double t)
{
    return segment.a + (segment.b - segment.a) * t;
}

inline SegmentBox segmentBox(const Segment& segment)
{
    return {
        (std::min)(segment.a.x, segment.b.x),
        (std::max)(segment.a.x, segment.b.x),
        (std::min)(segment.a.y, segment.b.y),
        (std::max)(segment.a.y, segment.b.y)
    };
}

inline bool boxesOverlap(const SegmentBox& lhs, const SegmentBox& rhs, double eps = splitEps)
{
    return lhs.minX <= rhs.maxX + eps
        && rhs.minX <= lhs.maxX + eps
        && lhs.minY <= rhs.maxY + eps
        && rhs.minY <= lhs.maxY + eps;
}

inline bool inUnit(double value)
{
    return value > -splitEps && value < 1.0 + splitEps;
}

inline double segmentParameter(const Segment& segment, const NfpPoint& pt)
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

inline void addParam(std::vector<double>& params, double value)
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

inline bool pointOnSegment(const NfpPoint& pt, const Segment& segment)
{
    NfpPoint ab = segment.b - segment.a;
    NfpPoint ap = pt - segment.a;
    if (!isEqual(ab.cross(ap), 0.0))
        return false;

    double t = segmentParameter(segment, pt);
    return inUnit(t);
}

inline double signedArea(const NfpPolyline& polygon)
{
    if (polygon.size() < 3)
        return 0.0;

    double area = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i)
        area += polygon[i].cross(polygon[(i + 1) % polygon.size()]);
    return area * 0.5;
}

inline NfpPoint leftRightTouchReferencePoint(const NfpPolyline& fixed, const NfpPolyline& moving)
{
    if (fixed.empty() || moving.empty())
        return {};

    const size_t fixedIndex = extremeVertexIndex<ExtremeVertex::LeftLowest>(fixed);
    const size_t movingIndex = extremeVertexIndex<ExtremeVertex::RightHighest>(moving);
    return fixed[fixedIndex] - (moving[movingIndex] - moving[0]);
}

inline double positiveTurnAngle(const NfpPoint& from, const NfpPoint& to)
{
    double angle = std::atan2(from.cross(to), from.dot(to));
    if (angle < 0.0)
        angle += 2.0 * pi;
    return angle;
}

inline bool vectorInCcwAngle(const NfpPoint& from, const NfpPoint& to, const NfpPoint& value)
{
    double span = positiveTurnAngle(from, to);
    double offset = positiveTurnAngle(from, value);
    return offset <= span + 1e-7;
}

inline bool angleEdgeCanTouch(const NfpPoint& prev, const NfpPoint& vertex, const NfpPoint& next, const NfpPoint& edge)
{
    NfpPoint inEdge = vertex - prev;
    NfpPoint outEdge = next - vertex;
    if (isEqual(inEdge.norm(), 0.0) || isEqual(outEdge.norm(), 0.0) || isEqual(edge.norm(), 0.0))
        return false;

    double vertexAngle = positiveTurnAngle(inEdge, outEdge);
    if (vertexAngle > pi + 1e-7)
        return false;

    return vectorInCcwAngle(-inEdge, -outEdge, edge);
}

inline double pointSegmentDistance(const NfpPoint& pt, const NfpPoint& a, const NfpPoint& b)
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

inline double polygonDistance(const NfpPolyline& fixed, const NfpPolyline& moving, const NfpPoint& movingReference, const NfpPoint& referencePosition)
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

inline std::vector<Segment> splitSegments(const std::vector<Segment>& segments)
{
    std::vector<std::vector<double>> params(segments.size());
    std::vector<SegmentBox> boxes;
    boxes.reserve(segments.size());
    for (auto& p : params) {
        p.push_back(0.0);
        p.push_back(1.0);
    }
    for (const auto& segment : segments)
        boxes.push_back(segmentBox(segment));

    for (size_t i = 0; i < segments.size(); ++i) {
        NfpPoint r = segments[i].b - segments[i].a;
        if (isEqual(r.norm(), 0.0))
            continue;

        for (size_t j = i + 1; j < segments.size(); ++j) {
            if (!boxesOverlap(boxes[i], boxes[j]))
                continue;

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

inline double intersectionArea(const CollisionContext& context, const NfpPoint& referencePosition)
{
    ClipperLib::Clipper clipper;
    clipper.AddPath(context.fixedPath, ClipperLib::ptSubject, true);
    clipper.AddPath(translatedPath(context.movingRelativePath, referencePosition), ClipperLib::ptClip, true);

    ClipperLib::Paths overlap;
    clipper.Execute(ClipperLib::ctIntersection, overlap, ClipperLib::pftNonZero, ClipperLib::pftNonZero);

    double area = 0.0;
    for (const auto& path : overlap) {
        area += std::fabs(ClipperLib::Area(path)) / (clipperScaler * static_cast<double>(clipperScaler));
    }
    return area;
}

inline double intersectionArea(const NfpPolyline& fixed, const NfpPolyline& moving, const NfpPoint& movingReference, const NfpPoint& referencePosition)
{
    return intersectionArea(makeCollisionContext(fixed, moving, movingReference), referencePosition);
}

inline std::vector<Segment> ringToSegments(const NfpPolyline& ring)
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

inline std::vector<NfpPolyline> sanitizeNfpRings(const std::vector<NfpPolyline>& rings, const NfpPolyline& fixed, const NfpPolyline& moving)
{
    std::vector<Segment> validEdges;
    if (fixed.size() < 3 || moving.size() < 3)
        return {};

    const NfpPoint reference = moving[0];
    const double areaTolerance = 1e-5;
    const CollisionContext collision = makeCollisionContext(fixed, moving, reference);

    for (const auto& ring : rings) {
        for (const auto& edge : ringToSegments(ring)) {
            NfpPoint mid = edge.a + (edge.b - edge.a) * 0.5;
            if (intersectionArea(collision, mid) <= areaTolerance)
                validEdges.push_back(edge);
        }
    }

    std::vector<NfpPolyline> sanitized = extractRings(validEdges);
    if (!sanitized.empty())
        return simplifyNfpRings(sanitized);

    return simplifyNfpRings(rings);
}

inline std::vector<Segment> generateCandidateSegments(const NfpPolyline& fixed, const NfpPolyline& moving)
{
    std::vector<Segment> segments;
    if (fixed.empty() || moving.empty())
        return segments;

    segments.reserve(fixed.size() * moving.size() * 2);
    const NfpPoint reference = moving[0];
    for (size_t i = 0; i < fixed.size(); ++i) {
        const NfpPoint& a0 = fixed[i];
        const NfpPoint& a1 = fixed[(i + 1) % fixed.size()];
        NfpPoint edge = a1 - a0;
        for (size_t j = 0; j < moving.size(); ++j) {
            const NfpPoint& prev = moving[(j + moving.size() - 1) % moving.size()];
            const NfpPoint& vertex = moving[j];
            const NfpPoint& next = moving[(j + 1) % moving.size()];
            if (!angleEdgeCanTouch(prev, vertex, next, edge))
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
            const NfpPoint& prev = fixed[(i + fixed.size() - 1) % fixed.size()];
            const NfpPoint& next = fixed[(i + 1) % fixed.size()];
            if (!angleEdgeCanTouch(prev, vertex, next, edge))
                continue;

            segments.push_back({ vertex - (b0 - reference), vertex - (b1 - reference) });
        }
    }

    return segments;
}

inline std::vector<Segment> filterBoundarySegments(const std::vector<Segment>& segments, const NfpPolyline& fixed, const NfpPolyline& moving)
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
    const CollisionContext collision = makeCollisionContext(fixed, moving, reference);

    std::set<std::pair<PointKey, PointKey>> seen;
    result.reserve(segments.size());
    for (const auto& segment : segments) {
        NfpPoint dir = segment.b - segment.a;
        double length = dir.norm();
        if (isEqual(length, 0.0))
            continue;

        NfpPoint unit = dir / length;
        NfpPoint left(-unit.y, unit.x);
        NfpPoint mid = segment.a + dir * 0.5;
        bool leftOverlaps = intersectionArea(collision, mid + left * probe) > areaEps;
        bool rightOverlaps = intersectionArea(collision, mid - left * probe) > areaEps;

        if (leftOverlaps == rightOverlaps)
            continue;

        Segment oriented = leftOverlaps ? segment : Segment { segment.b, segment.a };
        auto k = std::make_pair(keyOf(oriented.a), keyOf(oriented.b));
        if (seen.insert(k).second)
            result.push_back(oriented);
    }

    return result;
}

inline double turnAngle(const NfpPoint& from, const NfpPoint& to)
{
    return std::atan2(from.cross(to), from.dot(to));
}

inline bool pointKeyLessByPosition(const PointKey& lhs, const PointKey& rhs)
{
    if (lhs.second != rhs.second)
        return lhs.second < rhs.second;
    return lhs.first < rhs.first;
}

inline size_t chooseMinRotationEdge(const std::vector<Segment>& segments,
                                    const std::map<PointKey, std::vector<size_t>>& outgoing,
                                    const PointKey& pointKey,
                                    const NfpPoint& referenceDir,
                                    const std::vector<bool>& removed,
                                    const std::set<size_t>& localUsed)
{
    auto it = outgoing.find(pointKey);
    if (it == outgoing.end())
        return static_cast<size_t>(-1);

    size_t nextIndex = static_cast<size_t>(-1);
    double bestAngle = std::numeric_limits<double>::max();
    double bestLength = -1.0;
    for (size_t candidate : it->second) {
        if (removed[candidate] || localUsed.find(candidate) != localUsed.end())
            continue;

        NfpPoint candidateDir = segments[candidate].b - segments[candidate].a;
        if (isEqual(candidateDir.norm(), 0.0))
            continue;

        double angle = turnAngle(referenceDir, candidateDir);
        double length = candidateDir.norm();
        if (angle < bestAngle - 1e-10 || (isEqual(angle, bestAngle) && length > bestLength)) {
            bestAngle = angle;
            bestLength = length;
            nextIndex = candidate;
        }
    }

    return nextIndex;
}

inline bool traceRingFromFirstEdge(const std::vector<Segment>& segments,
                                   const std::map<PointKey, std::vector<size_t>>& outgoing,
                                   size_t firstIndex,
                                   const std::vector<bool>& removed,
                                   NfpPolyline& ring,
                                   std::vector<size_t>& path)
{
    ring.clear();
    path.clear();

    PointKey startKey = keyOf(segments[firstIndex].a);
    size_t currentIndex = firstIndex;
    NfpPoint referenceDir = segments[firstIndex].b - segments[firstIndex].a;
    std::set<size_t> localUsed;

    for (size_t guard = 0; guard < segments.size() + 1; ++guard) {
        const Segment& current = segments[currentIndex];
        path.push_back(currentIndex);
        localUsed.insert(currentIndex);

        if (ring.empty() || ring[ring.size() - 1] != current.a)
            ring.add(current.a);

        PointKey currentKey = keyOf(current.b);
        if (currentKey == startKey)
            return ring.size() >= 3;

        referenceDir = current.b - current.a;
        currentIndex = chooseMinRotationEdge(segments, outgoing, currentKey, referenceDir, removed, localUsed);
        if (currentIndex == static_cast<size_t>(-1))
            return false;
    }

    return false;
}

inline std::vector<NfpPolyline> extractRings(const std::vector<Segment>& segments)
{
    std::map<PointKey, std::vector<size_t>> outgoing;
    std::set<PointKey> outgoingPoints;
    for (size_t i = 0; i < segments.size(); ++i) {
        PointKey startKey = keyOf(segments[i].a);
        outgoing[startKey].push_back(i);
        outgoingPoints.insert(startKey);
    }

    if (segments.empty() || outgoingPoints.empty())
        return {};

    std::vector<bool> removed(segments.size(), false);
    std::vector<NfpPolyline> rings;

    PointKey outerStart = *std::min_element(outgoingPoints.begin(), outgoingPoints.end(), pointKeyLessByPosition);
    std::set<size_t> noLocalUsed;
    size_t firstOuter = chooseMinRotationEdge(segments, outgoing, outerStart, NfpPoint(1.0, 0.0), removed, noLocalUsed);
    if (firstOuter != static_cast<size_t>(-1)) {
        NfpPolyline outerRing;
        std::vector<size_t> outerPath;
        if (traceRingFromFirstEdge(segments, outgoing, firstOuter, removed, outerRing, outerPath)) {
            if (NfpPolyline::Clockwise == outerRing.orientation())
                outerRing.reverse();
            rings.push_back(outerRing);
            for (size_t index : outerPath)
                removed[index] = true;
        }
    }

    for (size_t startIndex = 0; startIndex < segments.size(); ++startIndex) {
        if (removed[startIndex])
            continue;

        NfpPolyline innerRing;
        std::vector<size_t> innerPath;
        if (!traceRingFromFirstEdge(segments, outgoing, startIndex, removed, innerRing, innerPath))
            continue;

        if (NfpPolyline::Clockwise != innerRing.orientation())
            continue;

        rings.push_back(innerRing);
        for (size_t index : innerPath)
            removed[index] = true;
    }

    std::sort(rings.begin(), rings.end(), [](const NfpPolyline& lhs, const NfpPolyline& rhs) {
        return std::fabs(signedArea(lhs)) > std::fabs(signedArea(rhs));
    });
    return rings;
}

inline std::vector<NfpPolyline> simplifyNfpRings(const std::vector<NfpPolyline>& rings)
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

    std::sort(result.begin(), result.end(), [](const NfpPolyline& lhs, const NfpPolyline& rhs) {
        return std::fabs(signedArea(lhs)) > std::fabs(signedArea(rhs));
    });

    return result.empty() ? rings : result;
}

inline std::vector<NfpPolyline> moveTouchingPointToRingStart(const std::vector<NfpPolyline>& rings,
                                                             const NfpPolyline& fixed,
                                                             const NfpPolyline& moving,
                                                             bool validateInnerStarts)
{
    if (fixed.size() < 3 || moving.size() < 3)
        return rings;

    std::vector<NfpPolyline> result = rings;
    const NfpPoint reference = moving[0];
    const NfpPoint guaranteedTouch = leftRightTouchReferencePoint(fixed, moving);
    const double overlapTolerance = 1e-5;
    std::optional<CollisionContext> collision;

    bool firstRing = true;
    for (auto& ring : result) {
        if (ring.size() < 2)
            continue;

        if (firstRing) {
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

        if (!validateInnerStarts)
            continue;

        if (!collision)
            collision.emplace(makeCollisionContext(fixed, moving, reference));

        size_t bestIndex = 0;
        double bestDistance = std::numeric_limits<double>::max();
        for (size_t i = 0; i < ring.size(); ++i) {
            double overlap = intersectionArea(*collision, ring[i]);
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
}

#endif // NFPPLACER_COMMON_H
