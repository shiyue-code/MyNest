#ifndef NEST2D_H
#define NEST2D_H

#include "nfpplacer.h"

namespace S_Shape2D {

class Nest2D {
public:
    using Polyline = S_Polyline2F;
    using Point = S_Point2F;
    using Container = std::vector<Polyline>;

public:
    Nest2D(const Container& c);

    void exec();

private:
    Container container;
};

}

#endif // NEST2D_H
