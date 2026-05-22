#ifndef S_DEF_H
#define S_DEF_H

#define USE_PREFIX_S_

#ifdef USE_PREFIX_S_
#define USE_S_(classname) using S_##classname = S_Shape2D::classname
#else
#define USE_S_(classname)
#endif

namespace S_Shape2D {

enum class ShapeType : short {
    ShapePoint,
    ShapePolyline,
};

enum ShapeFlags : short {
    FlagUndone = 1 << 0,
    FlagVisible = 1 << 1,
    FlagSelected = 1 << 2
};

}

#endif // S_DEF_H
