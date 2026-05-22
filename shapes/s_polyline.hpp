#ifndef S_POLYLINE_HPP
#define S_POLYLINE_HPP

#include "../s_common.hpp"
#include "s_point.hpp"
#include "s_shape.h"

#include <algorithm>
#include <deque>
#include <iterator>

namespace S_Shape2D {

template <typename T, typename Container = std::deque<Point<T>>>
class Polyline : public Shape {
public:
    using Point = S_Shape2D::Point<T>;
    using reference = Point&;
    using const_reference = const Point&;
    using Coord = typename Point::Coord;

    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;
    using size_type = typename Container::size_type;

    enum Orientation {
        Clockwise,
        CounterClockwise,
    };

public:
    Polyline() = default;
    Polyline(const Polyline&) = default;
    Polyline(Polyline&&) noexcept = default;
    Polyline& operator=(const Polyline&) = default;
    Polyline& operator=(Polyline&&) noexcept = default;

    ShapeType rtti() override
    {
        return ShapeType::ShapePolyline;
    }

    iterator begin()
    {
        return ctrlPoints.begin();
    }

    const_iterator begin() const
    {
        return ctrlPoints.begin();
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
        if (index == npos || index >= size()) {
            add(point);
            return;
        }

        ctrlPoints.insert(iteratorAt(index), point);
    }

    void remove(size_type index)
    {
        if (index < size())
            ctrlPoints.erase(iteratorAt(index));
    }

    void clear()
    {
        ctrlPoints.clear();
    }

    void reverse()
    {
        std::reverse(ctrlPoints.begin(), ctrlPoints.end());
    }

    reference operator[](size_type index)
    {
        return ctrlPoints.at(index);
    }

    const_reference operator[](size_type index) const
    {
        return ctrlPoints.at(index);
    }

    int orientation() const
    {
        return area() > Coord(0) ? CounterClockwise : Clockwise;
    }

    Coord area() const
    {
        const size_type count = size();
        if (count < 3)
            return Coord(0);

        Coord signedArea2x = Coord(0);
        for (size_type i = 0; i < count; ++i)
            signedArea2x += ctrlPoints[i].cross(ctrlPoints[(i + 1) % count]);

        return signedArea2x * Coord(0.5);
    }

    void rotate(Coord radian)
    {
        for (auto& pt : ctrlPoints)
            pt.rotate(radian);
    }

    void translate(const Point& offset)
    {
        for (auto& pt : ctrlPoints)
            pt.translate(offset);
    }

    bool operator==(const Polyline& poly) const
    {
        return size() == poly.size()
            && std::equal(begin(), end(), poly.begin(), [](const Point& lhs, const Point& rhs) {
                   return lhs == rhs;
               });
    }

    bool operator!=(const Polyline& poly) const
    {
        return !(*this == poly);
    }

private:
    iterator iteratorAt(size_type index)
    {
        auto iter = begin();
        std::advance(iter, static_cast<std::ptrdiff_t>(index));
        return iter;
    }

private:
    Container ctrlPoints;
    static constexpr size_type npos { static_cast<size_type>(-1) };
};

using Polyline2D = Polyline<double>;
using Polyline2F = Polyline<float>;
using Polyline2R = Polyline<Rational64>;

}

USE_S_(Polyline2D);
USE_S_(Polyline2F);
USE_S_(Polyline2R);

#endif // S_POLYLINE_HPP
