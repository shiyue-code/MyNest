#ifndef SHAPE_H
#define SHAPE_H

#include "s_def.h"

namespace S_Shape2D {

class Shape {
public:
    Shape() = default;

    virtual ShapeType rtti() = 0;

    ShapeFlags flags()
    {
        return ShapeFlags(shapeFlags);
    }

    bool hasFlag(ShapeFlags f)
    {
        return f & shapeFlags;
    }

    void addFlag(ShapeFlags f)
    {
        shapeFlags |= f;
    }

    void setFags(ShapeFlags f = FlagUndone)
    {
        shapeFlags = f;
    }

private:
    int shapeFlags;
};

}

USE_S_(Shape);

#endif // SHAPE_H
