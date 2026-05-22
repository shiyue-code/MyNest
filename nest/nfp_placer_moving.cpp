#include "nfp_placer_common.h"

#include <QDebug>

namespace S_Shape2D {

using namespace NfpDetail;

void NfpPlacer::exec()
{
    if (isExecute)
        return;
    nfps.clear();

    if (fixedPolygon.size() < 3 || movingPolygon.size() < 3) {
        isExecute = true;
        return;
    }

    Polyline fixedForValidation = fixedPolygon;
    Polyline movingForValidation = movingPolygon;

    const size_t fixedStartIndex = extremeVertexIndex<ExtremeVertex::LeftLowest>(fixedPolygon);
    const size_t movingContactIndex = extremeVertexIndex<ExtremeVertex::RightHighest>(movingPolygon);

    Polyline nfp;

    // Start with the fixed polygon's leftmost vertex touching the moving polygon's rightmost vertex.
    Touch start { fixedStartIndex, movingContactIndex, POINT_POINT };
    Touch curTouch = start;

    Point dir;
    Point ptBase = fixedPolygon[fixedStartIndex];
    nfp.add(ptBase);

    movingPolygon.translate(ptBase - movingPolygon[movingContactIndex]);

    Coord dis;
    do {
        dir = getDir(curTouch);

        Coord norm = dir.norm();
        if (isEqual(Coord(0), norm))
            break;

        dis = getDistance(dir / norm);

        if (isEqual(Coord(0), dis)) {
            break;
        } else if (norm <= dis) {
            ptBase.translate(dir);
        } else {
            auto d = (dir / norm) * dis;
            ptBase.translate(d);
        }

        for (const Point& pt : fixedPolygon) {
            if (pt == ptBase) {
                ptBase = pt;
                break;
            }
        }

        movingPolygon.translate(ptBase - movingPolygon[movingContactIndex]);
        nfp.add(ptBase);
        curTouch = getNextTouch(curTouch);
    } while (curTouch != Touch() && ptBase != fixedPolygon[fixedStartIndex]);

    auto offset = (movingPolygon[0] - movingPolygon[movingContactIndex]);
    for (Point& pt : nfp)
        pt += offset;

    nfps.push_back(nfp);
    finishNfpRings(nfps, fixedForValidation, movingForValidation, true);

    isExecute = true;
}

NfpPlacer::Touch NfpPlacer::getNextTouch(const NfpPlacer::Touch& curTouch)
{
    TouchVec res;

    size_t fixedSize = fixedPolygon.size();
    size_t movingSize = movingPolygon.size();

    size_t previousFixedIndex = fixedSize - 1;
    size_t previousMovingIndex = movingSize - 1;

    for (size_t fixedIndex = 0; fixedIndex < fixedSize; previousFixedIndex = fixedIndex++) {
        previousMovingIndex = movingSize - 1;

        for (size_t movingIndex = 0; movingIndex < movingSize; previousMovingIndex = movingIndex++) {
            int r = onSegment(movingPolygon[movingIndex], fixedPolygon[previousFixedIndex], fixedPolygon[fixedIndex]);
            if (r == 1) {
                res.push_back({ previousFixedIndex, movingIndex, POINT_POINT });
            } else if (r == -1) {
                res.push_back({ previousFixedIndex, movingIndex, EDGE_POINT });
            }

            r = onSegment(fixedPolygon[fixedIndex], movingPolygon[previousMovingIndex], movingPolygon[movingIndex]);
            if (r == -1)
                res.push_back({ fixedIndex, previousMovingIndex, POINT_EDGE });
        }
    }

    Point curDir = getDir(curTouch);
    Coord curNorm = curDir.norm();
    bool hasCurDir = !isEqual(Coord(0), curNorm);
    if (hasCurDir)
        curDir /= curNorm;

    double bestAngle = std::numeric_limits<double>::max();
    Coord bestDistance = -1;

    Touch rTouch {};
    for (Touch& t : res) {
        if (t == curTouch)
            continue;

        Point dirTmp = getDir(t);
        Coord normTmp = dirTmp.norm();
        if (isEqual(Coord(0), normTmp))
            continue;

        Point unitDir = dirTmp / normTmp;
        Coord disTmp = getDistance(unitDir);
        disTmp = std::min(disTmp, normTmp);
        if (isEqual(Coord(0), disTmp))
            continue;

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
    return rTouch;
}

NfpPlacer::Point NfpPlacer::getDir(const NfpPlacer::Touch& curTouch)
{
    size_t fixedNextIndex = (curTouch.fixedIndex + 1) % fixedPolygon.size();
    size_t movingNextIndex = (curTouch.movingIndex + 1) % movingPolygon.size();

    if (POINT_POINT == curTouch.state) {
        Point dir1 = fixedPolygon[fixedNextIndex] - fixedPolygon[curTouch.fixedIndex];
        Point dir2 = movingPolygon[movingNextIndex] - movingPolygon[curTouch.movingIndex];
        if (dir2.cross(dir1) > Coord(0))
            return dir1;
        return -dir2;
    }

    if (POINT_EDGE == curTouch.state)
        return (movingPolygon[curTouch.movingIndex] - movingPolygon[movingNextIndex]);

    if (EDGE_POINT == curTouch.state)
        return (fixedPolygon[fixedNextIndex] - fixedPolygon[curTouch.fixedIndex]);

    qDebug() << "ERROR";
    return {};
}

NfpPlacer::Coord NfpPlacer::getDistance(NfpPlacer::CPointRef dir)
{
    size_t ipre = fixedPolygon.size() - 1;
    Coord minDis = INT_MAX;
    Coord curDis = 0;

    for (size_t i = 0; i < fixedPolygon.size(); ipre = i++) {
        size_t jpre = movingPolygon.size() - 1;
        for (size_t j = 0; j < movingPolygon.size(); jpre = j++) {
            curDis = getProjectDis(fixedPolygon[i], movingPolygon[jpre], movingPolygon[j], -dir);
            if (curDis < minDis)
                minDis = curDis;

            curDis = getProjectDis(movingPolygon[j], fixedPolygon[ipre], fixedPolygon[i], dir);
            if (curDis < minDis)
                minDis = curDis;
        }
    }

    return minDis;
}

NfpPlacer::Coord NfpPlacer::getProjectDis(NfpPlacer::CPointRef pt,
                                           NfpPlacer::CPointRef segmentStart,
                                           NfpPlacer::CPointRef segmentEnd,
                                           NfpPlacer::CPointRef dir)
{
    Point edge = segmentEnd - segmentStart;
    Coord denom = dir.cross(edge);
    if (onSegment(pt, segmentStart, segmentEnd) == -1) {
        if (denom < Coord(-1e-7))
            return 0;
        return INT_MAX;
    }

    if (isEqual(denom, Coord(0)))
        return INT_MAX;

    Point delta = segmentStart - pt;
    Coord t = delta.cross(edge) / denom;
    Coord u = delta.cross(dir) / denom;

    if (t <= Coord(1e-7) || u < Coord(-1e-7) || u > Coord(1) + Coord(1e-7))
        return INT_MAX;

    return t;
}

int NfpPlacer::onSegment(NfpPlacer::CPointRef pt,
                         NfpPlacer::CPointRef segmentStart,
                         NfpPlacer::CPointRef segmentEnd)
{
    if (pt == segmentStart)
        return 1;

    if (pt == segmentEnd)
        return 0;

    Coord dis0 = (pt - segmentStart).square() + (pt - segmentEnd).square();
    Coord dis1 = (segmentStart - segmentEnd).square();
    if (isEqual(Coord(0), (pt - segmentStart).cross(pt - segmentEnd))
            && (isEqual(dis0, dis1) || dis0 < dis1)) {
        return -1;
    }

    return 0;
}

}
