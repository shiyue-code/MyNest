#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "s_shape.h"

namespace S_Shape2D {

class Document {
    using Shape = S_Shape;
    using ShapePtr = Shape*;
    using CShapePtr = const Shape*;

public:
    Document();

public:
    void add(CShapePtr shape);
    void remove(CShapePtr shape);

    void getShape(int index);
};

}

#endif // DOCUMENT_H
