#ifndef UTILTOOL_H
#define UTILTOOL_H

#include "s_box.hpp"
#include "s_point.hpp"
#include "s_polyline.hpp"

#include <cmath>

#define USE_CLIPPER

#ifdef USE_CLIPPER
#include "nest/clipper/clipper.hpp"

constexpr int clipperScaler = 1000000;
#endif

namespace S_Shape2D {

template <typename T>
inline Box<T> calcBoundingBox(const Polyline<T>& path)
{
    Box<T> box;
    for (const typename Polyline<T>::Point& pt : path)
        box.append(pt);
    return box;
}

template <typename T>
inline void cleanPolygon(Polyline<T>& path)
{
    for (size_t i = 1; i < path.size();) {
        if (path[i] == path[i - 1])
            path.remove(i);
        else
            ++i;
    }

    if (path.size() > 1 && path[0] == path[path.size() - 1])
        path.remove(path.size() - 1);
}

#ifdef USE_CLIPPER
template <typename T>
inline ClipperLib::Path polygon2Path(const Polyline<T>& polygon)
{
    ClipperLib::Path path;
    path.reserve(polygon.size());
    for (const auto& pt : polygon) {
        path.push_back({
            static_cast<ClipperLib::cInt>(std::llround(pt.x * clipperScaler)),
            static_cast<ClipperLib::cInt>(std::llround(pt.y * clipperScaler))
        });
    }

    return path;
}

template <typename T>
inline Polyline<T> path2Polygon(const ClipperLib::Path& path)
{
    Polyline<T> polygon;
    for (const auto& pt : path) {
        polygon.add({
            pt.X / static_cast<T>(clipperScaler),
            pt.Y / static_cast<T>(clipperScaler)
        });
    }

    return polygon;
}
#endif

}

#endif // UTILTOOL_H
