#ifndef NFP_PLACER_H
#define NFP_PLACER_H
#include "shapes/s_polyline.hpp"

#include <cstddef>
#include <vector>

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
        POINT_POINT, // fixed vertex touches moving vertex
        EDGE_POINT,  // fixed edge touches moving vertex
        POINT_EDGE,  // fixed vertex touches moving edge
    };

    struct Edge {
        size_t start = static_cast<size_t>(-1);
        size_t end = static_cast<size_t>(-1);
    };

    struct Touch {
        size_t fixedIndex = static_cast<size_t>(-1);
        size_t movingIndex = static_cast<size_t>(-1);
        int state = POINT_POINT; //TOUCH_STATE

        Touch() { }
        Touch(size_t fixed, size_t moving, int s)
            : fixedIndex(fixed)
            , movingIndex(moving)
            , state(s)
        {
        }

        bool operator==(const Touch& to) const
        {
            return fixedIndex == to.fixedIndex && movingIndex == to.movingIndex && state == to.state;
        }

        bool operator!=(const Touch& to) const
        {
            return !(*this == to);
        }
    };

    typedef std::vector<Touch> TouchVec;

public:
    NfpPlacer() = default;
    NfpPlacer(CPolylineRef fixed, CPolylineRef moving);

    void set(CPolylineRef fixed, CPolylineRef moving);

    void exec();
    void execVectorSegments();
    void execMinkowski();

    Container getNFPs();

public:
    Touch getNextTouch(const Touch& curTouch);
    Point getDir(const Touch& curTouch);
    Coord getDistance(CPointRef dir);

    Coord getProjectDis(CPointRef pt, CPointRef segmentStart, CPointRef segmentEnd, CPointRef dir);

    //@return -1 在线段上   0 不在线段上   1 在端点上
    int onSegment(CPointRef pt, CPointRef segmentStart, CPointRef segmentEnd);

private:
    Polyline fixedPolygon;
    Polyline movingPolygon;

    Container nfps;
    bool isExecute = false;
};

}
#endif // NFP_PLACER_H
