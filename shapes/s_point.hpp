#ifndef S_POINT_H
#define S_POINT_H

#include <vector>

#include "../s_common.hpp"
#include "s_math.h"
#include "s_shape.h"

namespace S_Shape2D {

template <typename T>
class Point : public Shape {
public:
    using PointT = Point<T>;
    using Coord = T;
    Coord x { 0 };
    Coord y { 0 };
    bool valid { false };

public:
    Point() = default;

    Point(Coord x, Coord y)
        : x(x)
        , y(y)
        , valid(true)
    {
    }

    Point(const Point& pt)
        : x(pt.x)
        , y(pt.y)
        , valid(pt.valid)
    {
    }

    operator bool() const
    {
        return valid;
    }

    //angle-X
    double angle() const
    {
        return Math::correctAngle(atan2(y, x));
    }

    double angleTo(const Point& pt) const
    {
        return (pt - *this).angle();
    }

    Coord dot(const Point& pt) const
    {
        return x * pt.x + y * pt.y;
    }

    Coord cross(const Point& pt) const
    {
        return x * pt.y - y * pt.x;
    }

    Coord distance2(const Point& pt) const
    {
        return (pt - *this).square();
    }

    Coord distance(const Point& pt) const
    {
        return (pt - *this).norm();
    }

    Coord norm() const
    {
        return sqrt(x * x + y * y);
    }

    Coord square() const
    {
        return x * x + y * y;
    }

    Point normalize()
    {
        *this /= norm();
        return *this;
    }

    Point operator-(const Point& pt) const
    {
        return { x - pt.x, y - pt.y };
    }

    Point operator-() const
    {
        return { -x, -y };
    }

    Point operator+(const Point& pt) const
    {
        return { x + pt.x, y + pt.y };
    }

    Point operator*(Coord v)
    {
        return { x * v, y * v };
    }

    Point& operator=(const Point& pt)
    {
        x = pt.x;
        y = pt.y;
        valid = pt.valid;
        return *this;
    }

    Point& operator-=(const Point& pt)
    {
        x -= pt.x;
        y -= pt.y;
        return *this;
    }

    Point& operator+=(const Point& pt)
    {
        x += pt.x;
        y += pt.y;
        return *this;
    }

    Point& operator*=(Coord v)
    {
        x *= v;
        y *= v;
        return *this;
    }

    Point operator/(Coord v)
    {
        auto pt = *this;
        pt /= v;
        return pt;
    }

    Point& operator/=(Coord v)
    {
        x /= v;
        y /= v;
        return *this;
    }

    bool operator==(const Point& pt) const
    {
        return valid && pt && isEqual(pt.x, x) && isEqual(pt.y, y);
    }

    bool operator!=(const Point& pt) const
    {
        return !(pt == *this);
    }

    void rotate(Coord radian)
    {
        Coord x_ = x * cos(radian) - y * sin(radian);
        Coord y_ = x * sin(radian) + y * cos(radian);
        x = x_;
        y = y_;
    }

    void translate(const Point& pt)
    {
        x += pt.x;
        y += pt.y;
    }

    // Shape interface
public:
    ShapeType rtti() override
    {
        return ShapeType::ShapePoint;
    }
};

typedef Point<double> Point2D;
typedef Point<float> Point2F;
typedef Point<Rational64> Point2R;
}

USE_S_(Point2D);
USE_S_(Point2F);
USE_S_(Point2R);

#endif // S_POINT_H
