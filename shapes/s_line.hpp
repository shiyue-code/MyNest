#ifndef S_LINE_HPP
#define S_LINE_HPP

#include "../s_common.hpp"
#include "s_point.hpp"
#include "s_shape.h"
namespace S_Shape2D {
template <typename T, typename UserMath=Math>
class Line : public Shape {

    using Point = Point<T>;

    Point s;
    Point e;

public:
    Line() { }
    Line(const Line &line){
        this->s = line.s;
        this->e = line.e;
    }

    Line(Point a, Point b) { s=a; e=b;}

    bool Intersect(Line v)
    {
        Line u(*this);
        return( (std::max(u.s.x,u.e.x)>=std::min(v.s.x,v.e.x))&&                     //排斥实验
            (std::max(v.s.x,v.e.x)>=std::min(u.s.x,u.e.x))&&
            (std::max(u.s.y,u.e.y)>=std::min(v.s.y,v.e.y))&&
            (std::max(v.s.y,v.e.y)>=std::min(u.s.y,u.e.y))&&
            (multiply(v.s,u.e,u.s)*multiply(u.e,v.e,u.s)>=0)&&         //跨立实验
            (multiply(u.s,v.e,v.s)*multiply(v.e,u.e,v.s)>=0));
    }


    Point Begin() {
        return s;
    }



};
}
#endif // S_LINE_HPP
