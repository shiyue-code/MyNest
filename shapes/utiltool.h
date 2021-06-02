#ifndef UTILTOOL_H
#define UTILTOOL_H

#include "s_box.hpp"
#include "s_point.hpp"
#include "s_polyline.hpp"

namespace S_Shape2D {

template <typename T>
static Box<T> calcBoundingBox(const Polyline<T>& path)
{
    Box<T> box;

    if (!path.empty()) {
        for (const typename Polyline<T>::Point& pt : path) {
            box.append(pt);
        }
    }

    return box;
}
}

#endif // UTILTOOL_H
