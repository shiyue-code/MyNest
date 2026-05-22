#ifndef S_BOX_HPP
#define S_BOX_HPP

#include "s_point.hpp"

#include <algorithm>

namespace S_Shape2D {

template <typename T>
struct PointPair {
    using PointType = Point<T>;

    PointType p0;
    PointType p1;

    PointPair() = default;
    PointPair(const PointType& p0, const PointType& p1)
        : p0(p0)
        , p1(p1)
    {
    }
};

template <typename T>
class Box : public PointPair<T> {
private:
    using Point = typename PointPair<T>::PointType;

public:
    Box() = default;

    Box(const Point& p0, const Point& p1)
    {
        set(p0, p1);
    }

    void set(const Point& p0, const Point& p1)
    {
        this->p0 = { std::min(p0.x, p1.x), std::min(p0.y, p1.y) };
        this->p1 = { std::max(p0.x, p1.x), std::max(p0.y, p1.y) };
        valid = true;
    }

    void append(const Point& pt)
    {
        if (!valid) {
            set(pt, pt);
            return;
        }

        left() = std::min(left(), pt.x);
        right() = std::max(right(), pt.x);
        top() = std::min(top(), pt.y);
        bottom() = std::max(bottom(), pt.y);
    }

    explicit operator bool() const
    {
        return valid;
    }

    Point leftTop() const
    {
        return this->p0;
    }

    Point rightBottom() const
    {
        return this->p1;
    }

    Point leftBottom() const
    {
        return { this->p0.x, this->p1.y };
    }

    Point rightTop() const
    {
        return { this->p1.x, this->p0.y };
    }

    Point center() const
    {
        return (this->p0 + this->p1) * T(0.5);
    }

    T width() const
    {
        return right() - left();
    }

    T height() const
    {
        return bottom() - top();
    }

    T left() const
    {
        return this->p0.x;
    }

    T right() const
    {
        return this->p1.x;
    }

    T top() const
    {
        return this->p0.y;
    }

    T bottom() const
    {
        return this->p1.y;
    }

    T& left()
    {
        return this->p0.x;
    }

    T& right()
    {
        return this->p1.x;
    }

    T& top()
    {
        return this->p0.y;
    }

    T& bottom()
    {
        return this->p1.y;
    }

    Box operator|(const Box& box) const
    {
        Box copy = *this;
        copy |= box;
        return copy;
    }

    Box& operator|=(const Box& box)
    {
        if (!box.valid)
            return *this;

        if (!valid) {
            this->p0 = box.p0;
            this->p1 = box.p1;
            valid = true;
            return *this;
        }

        left() = std::min(left(), box.left());
        right() = std::max(right(), box.right());
        top() = std::min(top(), box.top());
        bottom() = std::max(bottom(), box.bottom());
        return *this;
    }

private:
    bool valid { false };
};

using Box2D = Box<double>;

}

#endif // S_BOX_HPP
