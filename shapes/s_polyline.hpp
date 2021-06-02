#ifndef POLYLINE_H
#define POLYLINE_H

#include "../s_common.hpp"
#include "s_point.hpp"
#include "s_shape.h"

#include <deque>

namespace S_Shape2D {

template <typename T, typename Container = std::deque<Point<T>>>
class Polyline : public Shape {
public:
    using Point = Point<T>;
    using reference = Point&;
    using const_reference = const Point&;

    //Container
    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;
    using size_type = typename Container::size_type;

    // Shape interface
public:
    ShapeType
    rtti() override
    {
        return ShapeType::ShapePolyline;
    }

public:
    Polyline() { }

    Polyline(const Polyline& poly)
        : ctrlPoints(poly.ctrlPoints)
    {
    }

    Polyline(Polyline&& poly)
        : ctrlPoints(std::move(poly.ctrlPoints))
    {
    }

    iterator begin()
    {
        return ctrlPoints.begin();
    }
    const_iterator begin() const
    {
        return ctrlPoints.begin();
    }

    iterator iteratorAt(int index)
    {
        if (index > size())
            return end();
        auto iter = begin();
        std::advance(iter, index);
        return iter;
    }

    iterator end()
    {
        return ctrlPoints.end();
    }
    const_iterator end() const
    {
        return ctrlPoints.end();
    }

    size_type size() const
    {
        return ctrlPoints.size();
    }

    bool empty() const
    {
        return ctrlPoints.empty();
    }

    void add(const Point& pt)
    {
        ctrlPoints.push_back(pt);
    }

    void insert(const Point& point, size_type index = npos)
    {
        index == npos ? add(point) : (void)ctrlPoints.insert(iteratorAt(index), point);
    }

    void remove(size_type index)
    {
        ctrlPoints.erase(iteratorAt(index));
    }
    void clear()
    {
        ctrlPoints.clear();
    }

    reference operator[](size_type index)
    {
        return ctrlPoints.at(index);
    }

    const_reference operator[](size_type index) const
    {
        return ctrlPoints.at(index);
    }

    enum PolygonType {
        ConcavePolygon,
        ConvexPolygon,
    };
    int polygonType()
    {
        size_type nSize = this->size();
        for (size_type i = 0; i < nSize; ++i) {
            //        if()
        }
        return 0;
    }

    enum Orientation {
        Clockwise,
        CounterClockwise,
    };
    int orientation()
    {
        return area() > 0 ? CounterClockwise : Clockwise;
    }

    T area()
    {
        size_type nSize = this->size();
        if (nSize < 3)
            return 0;

        double a = 0;

        for (size_type i = 0, j = nSize - 1; i < nSize; ++i) {
            a += ((*this)[j].x + (*this)[i].x) * ((*this)[j].y - (*this)[i].y);
            j = i;
        }
        return -a * 0.5;
    }

    void rotate(T radian)
    {
        for (auto& pt : ctrlPoints) {
            pt.rotate(radian);
        }
    }

    void translate(const Point& ts)
    {
        for (auto& pt : ctrlPoints) {
            pt.translate(ts);
        }
    }

    Polyline& operator=(const Polyline& poly)
    {
        ctrlPoints = poly.ctrlPoints;
        return *this;
    }

private:
    Container ctrlPoints;

    static const size_type npos { static_cast<size_type>(-1) };
};

typedef Polyline<double> Polyline2D;
typedef Polyline<float> Polyline2F;
typedef Polyline<Rational64> Polyline2R;
}

USE_S_(Polyline2D);
USE_S_(Polyline2F);
USE_S_(Polyline2R);

#endif // POLYLINE_H
