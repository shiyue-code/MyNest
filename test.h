#ifndef TEST_H
#define TEST_H

#include "shapes/s_point.hpp"
#include "shapes/s_polyline.hpp"



inline S_Polyline2D GetTestP1()
{
    S_Polyline2D p1;
    p1.add({ 0, 0 });
    p1.add({ 100, 0});
    p1.add({ 100, 100 });
    p1.add({ 75, 100});
    p1.add({ 75, 20 });
    p1.add({ 25, 20 });
    p1.add({ 25, 100 });
    p1.add({ 0, 100 });
    return p1;
}

inline S_Polyline2D GetTestP2()
{
    S_Polyline2D p2;
    p2.add({ 0, 0 });
    p2.add({ 50, 0 });
    p2.add({ 50, 50 });
    p2.add({ 0, 50 });
    return p2;
}


//inline S_Polyline2D GetTestP1()
//{
//    S_Polyline2D p1;
//    p1.add({ 0, 0 });
//    p1.add({ 29.5, 27.6 });
//    p1.add({ 50, 0 });
//    p1.add({ 60, 69.8 });
//    p1.add({ 80, 69.8 });
//    p1.add({ 100, 0 });
//    p1.add({ 100, 100 });
//    p1.add({ 0, 100 });
//    return p1;
//}

//inline S_Polyline2D GetTestP2()
//{
//    S_Polyline2D p2;
//    p2.add({ 0, 0 });
//    p2.add({ 0, -25 });
//    p2.add({ -50, -50 });
//    p2.add({ 50, -50 });
//    return p2;
//}

//inline S_Polyline2D GetTestP1()
//{
//    S_Polyline2D p1;
//    p1.add({ 0, 0 });
//    p1.add({ 29.5, 27.6 });
//    p1.add({ 50, 0 });
//    p1.add({ 60, 69.8 });
//    p1.add({ 80, 69.8 });
//    p1.add({ 100, 0 });
//    p1.add({ 110.6, 20 });
//    p1.add({ 130.8, 10 });
//    p1.add({ 100, 100 });
//    p1.add({ 0, 100 });
//    return p1;
//}

#endif // TEST_H
