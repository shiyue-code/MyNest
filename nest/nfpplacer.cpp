#include "nfpplacer.h"

#include <QDebug>

namespace S_Shape2D {

NfpPlacer::NfpPlacer(NfpPlacer::CPolylineRef p1, NfpPlacer::CPolylineRef p2)
    : polyA(p1)
    , polyB(p2)
{
}

void NfpPlacer::exec()
{
    size_t minIndexA = -1;
    size_t maxIndexB = -1;

    //找到polyA X最小的点
    Coord minX = INT_MAX;
    Coord minY = INT_MAX;
    Coord maxX = INT_MIN;
    Coord maxY = INT_MIN;
    for (size_t i = 0; i < polyA.size(); ++i) {

        if (isEqual(minX, polyA[i].x) && minY > polyA[i].y) {
            minY = polyA[i].y;
            minIndexA = i;
        } else if (minX > polyA[i].x) {
            minX = polyA[i].x;
            minY = polyA[i].y;
            minIndexA = i;
        }
    }

    //找到polyB x最大的点
    for (size_t i = 0; i < polyB.size(); ++i) {

        if (isEqual(maxX, polyB[i].x) && maxY < polyB[i].y) {
            maxY = polyB[i].y;
            maxIndexB = i;
        } else if (maxX < polyB[i].x) {
            maxX = polyB[i].x;
            maxY = polyB[i].y;
            maxIndexB = i;
        }
    }

    Polyline nfp;

    //起始状态
    Touch start { minIndexA, maxIndexB, POINT_POINT };
    Touch curTouch = start;

    Point dir;
    Point ptBase = polyA[minIndexA];
    nfp.add(ptBase);

    //B 移动至起始状态
    polyB.translate(ptBase - polyB[maxIndexB]);

    Coord dis;

    do {
        dir = getDir(curTouch);

        Coord norm = dir.norm();
        dis = getDistance(dir / norm);

        if (isEqual(Coord(0), dis)) {
            break;
        } else if (norm < dis) {

            ptBase.translate(dir);
            //移动polyB
            polyB.translate(dir);
        } else {
            //加入
            auto d = (dir / norm) * dis;
            ptBase.translate(d);
            //移动polyB
            polyB.translate(d);
        }

        //check
        bool quit = false;
        for (const Point& pt : nfp) {
            if (pt == ptBase) {
                quit = true;
                break;
            }
        }

        nfp.add(ptBase);

        if (quit) {

            break;
        }

        qDebug() << "NFP" << ptBase.x << ptBase.y << dis << norm;
        curTouch = getNextTouch(curTouch);

        //        qDebug() << (curTouch != Touch()) << (ptBase != polyA[minIndexA]);
    } while (curTouch != start && curTouch != Touch() && ptBase != polyA[minIndexA]);

    nfps.push_back(nfp);
}

NfpPlacer::Container NfpPlacer::getNFPs()
{
    return nfps;
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

            //            qDebug() << "A:" << iB << iApre << iA;
            int r = onSegment(polyB[iB], polyA[iApre], polyA[iA]);
            if (r == 1) {
                res.push_back({ iApre, iB, POINT_POINT });
            } else if (r == -1) {
                res.push_back({ iApre, iB, POINT_EDGE });
            }

            //            qDebug() << "B:" << iA << iBpre << iB;
            r = onSegment(polyA[iA], polyB[iBpre], polyB[iB]);
            if (r == -1) {
                res.push_back({ iA, iBpre, EDGE_POINT });
            }
        }
    }

    //选取 MAX
    Coord disMax = -1;

    qDebug() << "Size" << res.size();
    Touch rTouch {};
    for (Touch& t : res) {

        Point dirTmp = getDir(t);
        qDebug() << "Touch" << t.indexA << t.indexB << dirTmp.x << dirTmp.y;
        Coord normTmp = dirTmp.norm();
        Coord disTmp = getDistance(dirTmp / normTmp);
        qDebug() << disTmp << normTmp << t.indexA << t.indexB << dirTmp.x << dirTmp.y;
        disTmp = std::min(disTmp, normTmp);

        if (disTmp > disMax) {
            rTouch = t;
            disMax = disTmp;
        }
    }

    return rTouch;
}

//NfpPlacer::Touch NfpPlacer::getNextTouch(const NfpPlacer::Touch& curTouch)
//{
//    Touch res;

//    size_t aSize = polyA.size();
//    size_t bSize = polyB.size();

//    size_t iA = (curTouch.indexA + 1) % aSize;
//    size_t iB = (curTouch.indexB + 1) % bSize;

//    size_t iApre = (iA - 1 + aSize) % aSize;
//    size_t iBpre = (iB - 1 + bSize) % bSize;

//    for (size_t i = 0; i < aSize; ++i) {

//        for (size_t j = 0; j < bSize; ++j) {

//            int r = onSegment(polyB[iB], polyA[iApre], polyA[iA]);
//            if (r == 1) {
//                return { iApre, iB, POINT_POINT };
//            } else if (r == -1) {
//                return { iApre, iB, EDGE_POINT };
//            }

//            r = onSegment(polyA[iA], polyB[iBpre], polyB[iB]);
//            if (r == -1) {
//                return { iA, iBpre, POINT_EDGE };
//            }

//            iB = iBpre;
//            iBpre = (iB - 1 + bSize) % bSize;
//        }

//        iB = curTouch.indexB;
//        iBpre = (iB - 1 + bSize) % bSize;
//        iA = iApre;
//        iApre = (iA - 1 + aSize) % aSize;
//    }

//    return res;
//}

NfpPlacer::Point NfpPlacer::getDir(const NfpPlacer::Touch& curTouch)
{
    //    int indexAPre = (curTouch.indexA - 1) % polyA.size();
    int indexANext = (curTouch.indexA + 1) % polyA.size();
    //    int indexBPre = (curTouch.indexB + polyB.size() - 1) % polyB.size();
    int indexBNext = (curTouch.indexB + 1) % polyB.size();

    if (POINT_POINT == curTouch.state) {

        Point dir1 = polyA[indexANext] - polyA[curTouch.indexA];
        Point dir2 = polyB[indexBNext] - polyB[curTouch.indexB];
        if (dir1.cross(dir2) < Coord(0)) {
            return dir1;
        } else {
            return (-dir2);
        }
    } else if (POINT_EDGE == curTouch.state) {
        return (polyA[indexANext] - polyB[curTouch.indexB]);
    } else if (EDGE_POINT == curTouch.state) {
        return (polyA[curTouch.indexA] - polyB[indexBNext]);
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
    for (size_t i = 0; i < polyA.size(); ipre = i++) {
        size_t jpre = polyB.size() - 1;
        for (size_t j = 0; j < polyB.size(); jpre = j++) {

            curDis = getProjectDis(polyA[i], polyB[jpre], polyB[j], -dir);
            //            qDebug() << "A" << i << jpre << j << curDis;
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
    //存在交点，计算距离
    if (p1 == pt || p2 == pt)
        return INT_MAX;

    //如果p1 p2 与dir同向
    Coord cc = (p2 - p1).cross(dir);
    if (isEqual(cc, Coord(0))) {
        return INT_MAX;
    }

    //顺时针
    if (cc < 0)
        return INT_MAX;

    //在线段上判断
    if (onSegment(pt, p1, p2) == -1 && cc < 0) {
        return INT_MAX;
    } else if (onSegment(pt, p1, p2) == -1 && cc > 0) {
        return 0;
    }

    Coord c1 = (p1 - pt).cross(dir);
    Coord c2 = (p2 - pt).cross(dir);

    if (isEqual(c1, Coord(0))) {
        Coord s = (p2 - pt).cross(p1 - pt);
        Coord h = (p2 - pt).cross(dir);

        return abs(s / h);
    }

    if (c1 * c2 < Coord(0)) {
        Point ptIn = intersect(pt, pt + dir, p1, p2);

        if (!ptIn) {
            return INT_MAX;
        }
        Coord s = (p1 - pt).cross(ptIn - pt);
        Coord h = (p1 - pt).cross(dir);

        if (isEqual(Coord(0), abs(h)))
            return 0;

        return abs(s / h);
    } else { //不用考虑距离
        return INT_MAX;
    }
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
    if (isEqual(Coord(0), (pt - p1).cross(pt - p2))
        && (pt - p1).square() < (p1 - p2).square()
        && (pt - p2).square() < (p1 - p2).square()) {
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
