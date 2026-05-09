#include "nfpplacer_common.h"

#include <QDebug>

namespace S_Shape2D {

using namespace NfpDetail;

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

    const size_t minIndexA = extremeVertexIndex<ExtremeVertex::LeftLowest>(polyA);
    const size_t maxIndexB = extremeVertexIndex<ExtremeVertex::RightHighest>(polyB);

    Polyline nfp;

    // Start with A's leftmost vertex touching B's rightmost vertex.
    Touch start { minIndexA, maxIndexB, POINT_POINT };
    Touch curTouch = start;

    Point dir;
    Point ptBase = polyA[minIndexA];
    nfp.add(ptBase);

    polyB.translate(ptBase - polyB[maxIndexB]);

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

        for (const Point& pt : polyA) {
            if (pt == ptBase) {
                ptBase = pt;
                break;
            }
        }

        polyB.translate(ptBase - polyB[maxIndexB]);
        nfp.add(ptBase);
        curTouch = getNextTouch(curTouch);
    } while (curTouch != Touch() && ptBase != polyA[minIndexA]);

    auto offset = (polyB[0] - polyB[maxIndexB]);
    for (Point& pt : nfp)
        pt += offset;

    nfps.push_back(nfp);
    finishNfpRings(nfps, fixedForCheck, movingForCheck, true);

    isExecute = true;
}

NfpPlacer::Touch NfpPlacer::getNextTouch(const NfpPlacer::Touch& curTouch)
{
    TouchVec res;

    size_t aSize = polyA.size();
    size_t bSize = polyB.size();

    size_t iApre = aSize - 1;
    size_t iBpre = bSize - 1;

    for (size_t iA = 0; iA < aSize; iApre = iA++) {
        iBpre = bSize - 1;

        for (size_t iB = 0; iB < bSize; iBpre = iB++) {
            int r = onSegment(polyB[iB], polyA[iApre], polyA[iA]);
            if (r == 1) {
                res.push_back({ iApre, iB, POINT_POINT });
            } else if (r == -1) {
                res.push_back({ iApre, iB, EDGE_POINT });
            }

            r = onSegment(polyA[iA], polyB[iBpre], polyB[iB]);
            if (r == -1)
                res.push_back({ iA, iBpre, POINT_EDGE });
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
    int indexANext = (curTouch.indexA + 1) % polyA.size();
    int indexBNext = (curTouch.indexB + 1) % polyB.size();

    if (POINT_POINT == curTouch.state) {
        Point dir1 = polyA[indexANext] - polyA[curTouch.indexA];
        Point dir2 = polyB[indexBNext] - polyB[curTouch.indexB];
        if (dir2.cross(dir1) > Coord(0))
            return dir1;
        return -dir2;
    }

    if (POINT_EDGE == curTouch.state)
        return (polyB[curTouch.indexB] - polyB[indexBNext]);

    if (EDGE_POINT == curTouch.state)
        return (polyA[indexANext] - polyA[curTouch.indexA]);

    qDebug() << "ERROR";
    return {};
}

NfpPlacer::Coord NfpPlacer::getDistance(NfpPlacer::CPointRef dir)
{
    size_t ipre = polyA.size() - 1;
    Coord minDis = INT_MAX;
    Coord curDis = 0;

    for (size_t i = 0; i < polyA.size(); ipre = i++) {
        size_t jpre = polyB.size() - 1;
        for (size_t j = 0; j < polyB.size(); jpre = j++) {
            curDis = getProjectDis(polyA[i], polyB[jpre], polyB[j], -dir);
            if (curDis < minDis)
                minDis = curDis;

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
    if (pt == p1)
        return 1;

    if (pt == p2)
        return 0;

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

    if (isEqual(Coord(0), d01))
        return {};

    Coord k = d02 / d01;
    Coord u = -d021 / d01;

    if ((isEqual(Coord(0), k) || k > 0) && u < 1 && u > 0)
        return p2 + (p3 - p2) * u;

    return {};
}

}
