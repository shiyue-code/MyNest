#ifndef TEST_H
#define TEST_H

#include "shapes/s_polyline.hpp"

inline S_Polyline2D createDefaultFixedPolygon()
{
    S_Polyline2D polygon;
    polygon.add({ 0, 0 });
    polygon.add({ 100, 0 });
    polygon.add({ 100, 100 });
    polygon.add({ 75, 100 });
    polygon.add({ 75, 20 });
    polygon.add({ 25, 20 });
    polygon.add({ 25, 100 });
    polygon.add({ 0, 100 });
    return polygon;
}

inline S_Polyline2D createDefaultMovingPolygon()
{
    S_Polyline2D polygon;
    polygon.add({ 0, 0 });
    polygon.add({ 50, 0 });
    polygon.add({ 50, 50 });
    polygon.add({ 0, 50 });
    return polygon;
}

#endif // TEST_H
