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

        if ( (isEqual(minX, polyA[i].x) && minY > polyA[i].y )
             || minX > polyA[i].x) {
            minY = polyA[i].y;
            minX = polyA[i].x;
            minIndexA = i;
        }
    }

    //找到polyB x最大的点
    for (size_t i = 0; i < polyB.size(); ++i) {

        if ( (isEqual(maxX, polyB[i].x) && maxY < polyB[i].y)
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
                res.push_back({ iApre, iB, POINT_EDGE });
            }

            r = onSegment(polyA[iA], polyB[iBpre], polyB[iB]);
            //            qDebug() << "B:" << iA << iBpre << iB << r;
            if (r == -1) {
                res.push_back({ iA, iBpre, EDGE_POINT });
            }
        }
    }

    //选取 MAX
    Coord disMax = -1;

    //    qDebug() << "Size" << res.size();
    Touch rTouch {};
    for (Touch& t : res) {

        Point dirTmp = getDir(t);
        //        qDebug() << "Touch" << t.indexA << t.indexB << t.state << dirTmp.x << dirTmp.y;
        Coord normTmp = dirTmp.norm();
        Coord disTmp = getDistance(dirTmp / normTmp);
        //        qDebug() << disTmp << normTmp << t.indexA << t.indexB << dirTmp.x << dirTmp.y;
        disTmp = std::min(disTmp, normTmp);

        if (disTmp > disMax) {
            rTouch = t;
            disMax = disTmp;
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
        if (dir1.cross(dir2) <= Coord(0)) {
            return dir1;
        } else {
            return (-dir2);
        }
    } else if (POINT_EDGE == curTouch.state) { //点边相接 其中边为（polyB[indexBNext] - polyB[curTouch.indexB]）
        return (polyA[indexANext] - polyB[curTouch.indexB]);
    } else if (EDGE_POINT == curTouch.state) { //边点相接 其中边为 polyA[indexANext] - polyA[curTouch.indexA]
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

    //如果p1 p2 与 dir平行
    Coord cc = (p2 - p1).cross(dir);
    if (isEqual(cc, Coord(0))) {
        return INT_MAX;
    }

    //顺时针
    if (cc < 0)
        return INT_MAX;

    //    Coord dt = (p2 - p1).dot(dir);

    //    qDebug() << "Dot" << dt;
    //    if (p1 == pt && dt > 0) {
    //        return 0;
    //    } else if (p1 == pt && dt < 0) {
    //        return INT_MAX;
    //    } else if (p2 == pt && dt < 0) {
    //        return 0;
    //    } else if (p2 == pt && dt > 0) {
    //        return INT_MAX;
    //    }

    //在线段上判断点的
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

    if (p1 == pt || p2 == pt)
        return INT_MAX;

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
