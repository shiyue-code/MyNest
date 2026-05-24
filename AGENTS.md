# AGENTS.md

## Project Overview

QtCalcauteNFPs is a C++ Qt Widgets application for computing and visualizing No-Fit Polygons (NFP) for 2D irregular polygon nesting. It calculates NFPs using three different algorithms and provides animation for verification.

## Build System

**Toolchain:** Qt 5.15.2 MinGW 64-bit with MinGW 8.1.0 (C++17)

**Build commands (PowerShell):**
```powershell
mkdir build-qt5
cd build-qt5
D:\Qt\5.15.2\mingw81_64\bin\qmake.exe ..\QtCalcauteNFPs.pro
D:\Qt\Tools\mingw810_64\bin\mingw32-make.exe -j4
```

**Qt Creator build:**
1. Open `QtCalcauteNFPs.pro` in Qt Creator.
2. Select a kit such as `Desktop Qt 5.15.2 MSVC2019 64bit` or `Desktop Qt 5.15.2 MinGW 64-bit`.
3. Run `Build > Run qmake` after opening the project or changing `.pro`.
4. Run `Build > Build Project "QtCalcauteNFPs"`.

**Important:** After adding/removing `.cpp` files, re-run `qmake` to regenerate the Makefile.

## Source Structure

```
├── main.cpp              # Entry point → Widget
├── widget.cpp/h/ui       # Main UI, algorithm selection, debug output
├── s_common.hpp          # Common math utilities, epsilon comparisons
├── nest/                 # NFP algorithms
│   ├── nfp_placer.h/cpp   # NfpPlacer interface (set, exec, getNFPs)
│   ├── nfp_placer_common.h # Shared geometry, segment splitting, ring extraction
│   ├── nfp_placer_moving.cpp  # Moving collision algorithm
│   ├── nfp_placer_vector.cpp  # Vector segment algorithm
│   ├── nfp_placer_minkowski.cpp # Minkowski algorithm (uses Clipper)
│   └── clipper/          # Clipper library (Minkowski diff)
├── shapes/               # 2D geometry primitives
│   ├── s_point.hpp       # Point<T> template (Point2D, Point2F, Point2R)
│   ├── s_polyline.hpp    # Polyline<T> template (Polyline2D, Polyline2R)
│   ├── s_box.hpp         # Bounding box
│   ├── s_math.h          # Math utilities
│   └── s_shape.h         # Shape base class
├── view/                 # Visualization
│   ├── myctrlview.cpp/h  # NFP drawing, animation, reference point display
│   └── kwctrlview.cpp/h  # Base view control
```

## Key Conventions

**Namespace:** All geometry types are in `S_Shape2D` namespace. Type aliases use `USE_S_` macro (e.g., `S_Point2D`, `S_Polyline2D`).

**Coordinate types:**
- `double` for general calculations (Point2D, Polyline2D)
- `Rational<long long>` for exact arithmetic (Point2R, Polyline2R)

**NFP algorithms:**
1. **Moving collision** (`exec()`) - contact-state advancement
2. **Vector segments** (`execVectorSegments()`) - angle-vector edge approach
3. **Minkowski** (`execMinkowski()`) - uses Clipper's MinkowskiDiff

**Debug output:** The application prints polygon vertices, algorithm timing, and segment statistics to Qt's debug output when calculating NFPs.

## Dependencies

- Qt 5.15.2 (core, gui, widgets)
- OpenGL (linked via `-lopengl32 -lglu32`)
- Clipper library (bundled in `nest/clipper/`)
