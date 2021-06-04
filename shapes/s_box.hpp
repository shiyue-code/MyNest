#ifndef BOX_H
#define BOX_H

#include "s_point.hpp"
#include "s_shape.h"

namespace S_Shape2D {
template <typename T>
struct PointPair {
    typedef Point<T> PointType;

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
    bool valid { false };

    typedef PointPair<T> BoxBase;
    typedef typename PointPair<T>::PointType Point;

public:
    Box() = default;
    Box(const Point& p0, const Point& p1)
        : BoxBase({ std::min(p0.x, p1.x), std::min(p0.y, p1.y) },
            { std::max(p0.x, p1.x), std::max(p0.y, p1.y) })
        , valid(true)
    {
    }

    Box(const Box<T>& box)
        : valid(box.valid)
    {
        this->p0 = box.p0;
        this->p1 = box.p1;
    }

    void set(const Point& p0, const Point& p1)
    {
        this->p0 = { std::min(p0.x, p1.x), std::min(p0.y, p1.y) };
        this->p1 = { std::max(p0.x, p1.x), std::max(p0.y, p1.y) };
        valid = true;
    }

    void append(const Point& pt)
    {
        if (valid) {
            if (left() > pt.x)
                left() = pt.x;
            else if (right() < pt.x)
                right() = pt.x;

            if (top() > pt.y)
                top() = pt.y;
            if (bottom() < pt.y)
                bottom() = pt.y;
        } else {
            set(pt, pt);
        }
    }

    operator bool() const
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
        return (this->p0 + this->p1) * 0.5;
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

    Box<T> operator|(const Box<T>& box)
    {
        Box copy = *this;
        copy |= box;
        return copy;
    }

    Box<T>& operator=(const Box<T>& box)
    {
        this->p0 = box.p0;
        this->p1 = box.p1;
        this->valid = box.valid;
        return *this;
    }

    Box<T>& operator|=(const Box<T>& box)
    {

        if (valid && box.valid) {
            if (this->left() > box.left())
                this->left() = box.left();

            if (this->right() < box.right())
                this->right() = box.right();

            if (this->top() > box.top())
                this->top() = box.top();

            if (this->bottom() < box.bottom())
                this->bottom() = box.bottom();
        } else {
            *this = box;
        }
        return *this;
    }
};

typedef Box<double> Box2D;

}

#endif // BOX_H
