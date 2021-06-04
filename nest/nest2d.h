#ifndef NEST2D_H
#define NEST2D_H

#include "nfpplacer.h"
#include "shapes/s_box.hpp"

namespace S_Shape2D {

struct Item {
    double x;
    double y;
    double rotate;
};

class Nest2D {

public:
    using Coord = double;
    using Polyline = Polyline<Coord>;
    using Box = Box<Coord>;
    using Point = Polyline::Point;

    //图形列表
    using Container = std::vector<Polyline>;

public:
    Nest2D(const Container& c);

    void exec();

private:
private:
    Container container;
};

class NestSingle2D {
public:
    using Coord = double;
    using Polyline = Polyline<Coord>;
    using Box = Box<Coord>;
    using Point = Polyline::Point;

    //图形列表
    using Container = std::vector<Polyline>;

    enum FirstPlacer {
        LeftTop,
        LeftBottom,
    };

public:
    NestSingle2D(const Box& box, const Polyline& poly)
        : box(box)
        , poly(poly)
    {
        polyR = poly;
        polyR.rotate(pi);

        getPlacer(Poly_Poly).set(poly, poly);
        getPlacer(PolyR_PolyR).set(polyR, polyR);
        getPlacer(Poly_PolyR).set(poly, polyR);
        getPlacer(PolyR_Poly).set(polyR, poly);
    }

    void exec();

    Container result();

private:
    void placerFisrt(int firstPlacer);

    enum {
        Poly_PolyR,
        PolyR_Poly,
        Poly_Poly,
        PolyR_PolyR,
    };
    NfpPlacer& getPlacer(int type);

private:
    Box box;
    Polyline poly;
    Polyline polyR;

    Container resultList;

    std::map<int, NfpPlacer> placerMap;

    double x_space = 0;
    double xr_start = 0;
    double xr_space = 0;
    double y_space = 0;
    double yr_space = 0;
};

}

#endif // NEST2D_H
