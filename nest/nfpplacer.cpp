#include "nfpplacer.h"

#include <QDebug>

namespace S_Shape2D {

NfpPlacer::NfpPlacer(NfpPlacer::CPolylineRef p1, NfpPlacer::CPolylineRef p2)
    : polyA(p1)
    , polyB(p2)
{
}

void NfpPlacer::set(NfpPlacer::CPolylineRef A, NfpPlacer::CPolylineRef B)
{
    polyA = A;
    polyB = B;

    isExecute = false;
}

void NfpPlacer::exec()
{
    if (isExecute)
        return;

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

        for (const Point& pt : polyA) {
            if (pt == ptBase) {
                ptBase = pt;
                break;
            }
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

        //        qDebug() << "NFP" << ptBase.x << ptBase.y << dis << norm;
        curTouch = getNextTouch(curTouch);

        //        qDebug() << (curTouch != Touch()) << (ptBase != polyA[minIndexA]);
    } while (curTouch != start && curTouch != Touch() && ptBase != polyA[minIndexA]);

    auto offset = (polyB[0] - polyB[maxIndexB]);
    for (Point& pt : nfp) {
        pt += offset;
    }

    nfps.push_back(nfp);

    isExecute = true;
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
        qDebug() << "Touch" << t.indexA << t.indexB << t.state << dirTmp.x << dirTmp.y;
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
    int indexBNext = (cur