#ifndef NFPPLACER_H
#define NFPPLACER_H
#include "shapes/s_polyline.hpp"

namespace S_Shape2D {

class NfpPlacer {

public:
    using Polyline = S_Polyline2D;
    using Point = Polyline::Point;

    using CPointRef = const Point&;
    using CPolylineRef = const Polyline&;
    using Coord = Point::Coord;
    using Container = std::vector<Polyline>;

private:
    enum TOUCH_STATE {
        POINT_POINT, //A的顶点和B的顶点相接触
        EDGE_POINT,  //A的边和B的点相接处
        POINT_EDGE,  //A的顶点B的边相接触
    };

    struct Edge {
        size_t start = static_cast<size_t>(-1);
        size_t end = static_cast<size_t>(-1);
    };

    struct Touch {
        size_t indexA = static_cast<size_t>(-1);
        size_t indexB = static_cast<size_t>(-1);
        int state = POINT_POINT; //TOUCH_STATE

        Touch() { }
        Touch(size_t A, size_t B, int s)
            : indexA(A)
            , indexB(B)
            , state(s)
        {
        }

        bool operator==(const Touch& to) const
        {
            return indexA == to.indexA && indexB == to.indexB && state == to.state;
        }

        bool operator!=(const Touch& to) const
        {
            return !(*this == to);
        }
    };

    typedef std::vector<Touch> TouchVec;

public:
    NfpPlacer() = default;
    NfpPlacer(CPolylineRef A, CPolylineRef B);

    void set(CPolylineRef A, CPolylineRef B);

    void exec();

    Container getNFPs();

public:
    Touch getNextTouch(const Touch& curTouch);
    Point getDir(const Touch& curTouch);
    Coord getDistance(CPointRef dir);

    Coord getProjectDis(CPointRef pt, CPointRef p1, CPointRef p2, CPointRef dir);

    //@return -1 在线段上   0 不在线段上   1 在端点上
    int onSegment(CPointRef pt, CPointRef p1, CPointRef p2);

    // line1: p0 p1
    // line2: p2 p3
    Point intersect(CPointRef p0, CPointRef p1, CPointRef p2, CPointRef p3);

private:
    Polyline polyA;
    Polyline polyB;

    Container nfps;
    bool isExecute = false;
};

}
#endif // NFPPLACER_H
