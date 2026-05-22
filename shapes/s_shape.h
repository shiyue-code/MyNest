#ifndef S_SHAPE_H
#define S_SHAPE_H

#include "s_def.h"

namespace S_Shape2D {

class Shape {
public:
    Shape() = default;
    virtual ~Shape() = default;

    virtual ShapeType rtti() = 0;

    ShapeFlags flags() const
    {
        return ShapeFlags(shapeFlags);
    }

    bool hasFlag(ShapeFlags flag) const
    {
        return (shapeFlags & flag) != 0;
    }

    void addFlag(ShapeFlags flag)
    {
        shapeFlags |= flag;
    }

    void setFlags(ShapeFlags flags = FlagUndone)
    {
        shapeFlags = flags;
    }

private:
    int shapeFlags = FlagUndone;
};

}

USE_S_(Shape);

#endif // S_SHAPE_H
