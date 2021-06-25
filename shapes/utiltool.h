#ifndef UTILTOOL_H
#define UTILTOOL_H

#include "s_box.hpp"
#include "s_point.hpp"
#include "s_polyline.hpp"

#define USE_CLIPPER

#ifdef USE_CLIPPER
#include "nest/clipper/clipper.hpp"

constexpr int clipperScaler = 1000000;
#endif

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

/*!
 * Remove duplicate points
 */

template <typename T>
static void cleanPolygon(Polyline<T>& path)
{
    typename Polyline<T>::Point ptTmp;
    size_t i = 1;
    for (; i < path.size();) {
        if (path[i] == path[i - 1]) {
            path.remove(i);
        } else {
            ++i;
        }
    }
}

template <typename T>
static Point<T> intersect(const Point<T>& p0, const Point<T>& p1, const Point<T>& p2, const Point<T>& p3)
{
    T d01 = (p1 - p0).cross(p3 - p2);
    T d02 = (p2 - p0).cross(p3 - p2);
    T d021 = (p0 - p2).cross(p1 - p0);

    if (isEqual(T(0), d01)) { //平行
        return {};
    }
    // pt = p0 + k* (p1-p0)
    T k = d02 / d01;
    // pt = p2 + u* (p3-p2)
    T u = -d021 / d01;

    if (isEqual(T(0), k)) {
        return p0;
    } else if (isEqual(T(1), k)) {
        return p1;
    } else if (isEqual(T(0), u)) {
        return p2;
    } else if (isEqual(T(1), u)) {
        return p3;
    } else if (k < 1 && k > 0 && u < 1 && u > 0) {
        return p2 + (p3 - p2) * u;
    }

    return {};
}

#ifdef USE_CLIPPER
template <typename T>
static ClipperLib::Path polygon2Path(const Polyline<T>& polygon)
{
    ClipperLib::Path path;
    for (const auto& pt : polygon) {
        path.push_back({ static_cast<ClipperLib::cInt>(pt.x * clipperScaler),
            static_cast<ClipperLib::cInt>(pt.y * clipperScaler) });
    }

    return path;
}

template <typename T>
static Polyline<T> path2Polygon(const ClipperLib::Path& path)
{
    Polyline<T> polygon;
    for (const auto& pt : path) {
        polygon.add({ pt.X / static_cast<T>(clipperScaler),
            pt.Y / static_cast<T>(clipperScaler) });
    }

    return polygon;
}

static ClipperLib::Paths Intersection(const ClipperLib::Path& p1, const ClipperLib::Path& p2)
{
    ClipperLib::Clipper clipper;
    clipper.AddPath(p1, ClipperLib::PolyType::ptSubject, true);
    clipper.AddPath(p2, ClipperLib::PolyType::ptClip, true);

    ClipperLib::Paths combinedNfp;
    clipper.Execute(ClipperLib::ClipType::ctIntersection,
        combinedNfp,
        ClipperLib::PolyFillType::pftNonZero,
        ClipperLib::PolyFillType::pftNonZero);

    return combinedNfp;
}

static ClipperLib::Paths Union(const ClipperLib::Paths& paths)
{
    ClipperLib::Clipper clipper;
    clipper.AddPaths(paths, ClipperLib::PolyType::ptClip, true);

    ClipperLib::Paths combinedNfp;
    clipper.Execute(ClipperLib::ClipType::ctUnion,
        combinedNfp,
        ClipperLib::PolyFillType::pftNonZero,
        ClipperLib::PolyFillType::pftNonZero);
    return combinedNfp;
}

static ClipperLib::Paths Difference(const ClipperLib::Path& p1, const ClipperLib::Path& p2)
{
    ClipperLib::Clipper clipper;
    clipper.AddPath(p1, ClipperLib::PolyType::ptSubject, true);
    clipper.AddPath(p2, ClipperLib::PolyType::ptClip, true);

    ClipperLib::Paths combinedNfp;
    clipper.Execute(ClipperLib::ClipType::ctDifference,
        combinedNfp,
        ClipperLib::PolyFillType::pftNonZero,
        ClipperLib::PolyFillType::pftNonZero);
    return combinedNfp;
}
#endif

}

#endif // UTILTOOL_H
