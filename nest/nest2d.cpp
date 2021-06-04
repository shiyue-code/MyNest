#include "nest2d.h"

#include "nfpplacer.h"
#include "shapes/utiltool.h"

namespace S_Shape2D {

Nest2D::Nest2D(const Container& c)
    : container(c)
{
}

void Nest2D::exec() { }

/**
*NestSingle2D
*/
void NestSingle2D::exec()
{
    placerFisrt(LeftTop);
}

NestSingle2D::Container NestSingle2D::result()
{
    return resultList;
}

void NestSingle2D::placerFisrt(int firstPlacer)
{
    auto polytmp = poly;
    Box polyBox = calcBoundingBox(poly);
    if (firstPlacer == LeftTop) {
        auto offset = box.leftBottom() - polyBox.leftBottom();
        polytmp.translate(offset);

    } else {

        auto offset = box.leftTop() - polyBox.leftTop();
        polytmp.translate(offset);
    }

    resultList.push_back(polytmp);

    ///计算 xspace
    getPlacer(Poly_Poly).exec();

    auto nfps = getPlacer(Poly_Poly).getNFPs();
    auto nfp1 = nfps[0];

    Point ptTmp;

    for (size_t i = 1; i < nfp1.size(); ++i) {
        Point p1 = poly[0];
        p1.x += box.width();

        Point ptI = intersect(poly[0], p1, nfp1[i - 1], nfp1[i]);

        if ((ptI && ptTmp && ptTmp.x < ptI)
            || (ptI && !ptTmp)) {
            ptTmp = ptI;
        }
    }

    x_space = ptTmp.x - poly[0].x;

    getPlacer(Poly_PolyR).exec();
    nfps = getPlacer(Poly_PolyR).getNFPs();
    auto nfp2 = nfps[0];
    auto nfp2Tmp = nfps[0];

    auto path1 = polygon2Path(nfp2);

    nfp2Tmp.translate({ x_space, 0 });
    auto path2 = polygon2Path(nfp2Tmp);

    auto path = Difference(path1, path2);

    auto differencePoly = path2Polygon<Coord>(path[0]);

    double ymin = INT_MAX;
    for (const auto& pt : differencePoly) {
        if (pt.x > box.left() && pt.y > poly[0].y) {
            if (pt.y < ymin) {
                ymin = pt.y;
                xr_start = pt.x;
            }
        }
    }

    auto dpBox = calcBoundingBox(differencePoly);

    if (isEqual(dpBox.top(), xr_start)) {
        xr_start = 0;
    }
    y_space = 0;

    qDebug() << x_space << xr_start;

    resultList.push_back(nfp2Tmp);
    resultList.push_back(nfp2);
}

NfpPlacer& NestSingle2D::getPlacer(int type)
{
    return placerMap[type];
}

}
