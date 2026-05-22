#ifndef S_POINT_HPP
#define S_POINT_HPP

#include "../s_common.hpp"
#include "s_math.h"
#include "s_shape.h"

#include <cmath>

namespace S_Shape2D {

template <typename T, typename UserMath = Math>
class Point : public Shape {
public:
    using PointT = Point<T, UserMath>;
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

    explicit operator bool() const
    {
        return valid;
    }

    double angle() const
    {
        return UserMath::correctAngle(std::atan2(y, x));
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
        return std::sqrt(x * x + y * y);
    }

    Coord square() const
    {
        return x * x + y * y;
    }

    Point normalize()
    {
        Coord length = norm();
        if (!isEqual(length, Coord(0)))
            *this /= length;
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

    Point operator*(Coord value) const
    {
        return { x * value, y * value };
    }

    Point operator/(Coord value) const
    {
        auto pt = *this;
        pt /= value;
        return pt;
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

    Point& operator*=(Coord value)
    {
        x *= value;
        y *= value;
        return *this;
    }

    Point& operator/=(Coord value)
    {
        x /= value;
        y /= value;
        return *this;
    }

    bool operator==(const Point& pt) const
    {
        return valid && pt.valid && isEqual(pt.x, x) && isEqual(pt.y, y);
    }

    bool operator!=(const Point& pt) const
    {
        return !(*this == pt);
    }

    void rotate(Coord radian)
    {
        Coord rotatedX = x * std::cos(radian) - y * std::sin(radian);
        Coord rotatedY = x * std::sin(radian) + y * std::cos(radian);
        x = rotatedX;
        y = rotatedY;
    }

    void translate(const Point& offset)
    {
        x += offset.x;
        y += offset.y;
    }

    ShapeType rtti() override
    {
        return ShapeType::ShapePoint;
    }
};

using Point2D = Point<double>;
using Point2F = Point<float>;
using Point2R = Point<Rational64>;

}

USE_S_(Point2D);
USE_S_(Point2F);
USE_S_(Point2R);

#endif // S_POINT_HPP
