#ifndef ACTIONS_H
#define ACTIONS_H

#include "shapes/s_point.hpp"

namespace S_Shape2D {

template <typename PointType>
class Action {

    using Point = PointType;
    using PointRef = Point&;
    using CPointRef = const Point&;

public:
    Action();

    virtual int type() = 0;

    virtual void toggle() = 0;
    virtual void suspend() = 0;

    virtual void mousePressed(CPointRef point);
    virtual void mouseRelease(CPointRef point);
    virtual void mouseMove(CPointRef point);
    virtual void mouseWheel(CPointRef point, double r);

    virtual void redo();
    virtual void undo();

private:
};

}
#endif // ACTIONS_H
