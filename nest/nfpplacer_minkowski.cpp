#include "nfpplacer_common.h"

#include <QDebug>

namespace S_Shape2D {

using namespace NfpDetail;

void NfpPlacer::execMinkowski()
{
    if (isExecute)
        return;
    nfps.clear();

    Polyline fixed = polyA;
    Polyline moving = polyB;
    if (!normalizeNfpInputs(polyA, polyB, fixed, moving)) {
        isExecute = true;
        return;
    }

    ClipperPaths solution;
    ClipperLib::MinkowskiDiff(relativePath(moving, moving[0]), polygon2Path(fixed), solution);
    ClipperLib::CleanPolygons(solution, 1.0);

    std::sort(solution.begin(), solution.end(), [](const ClipperPath& lhs, const ClipperPath& rhs) {
        return std::fabs(ClipperLib::Area(lhs)) > std::fabs(ClipperLib::Area(rhs));
    });

    for (const auto& path : solution) {
        if (path.size() < 3 || isEqual(std::fabs(ClipperLib::Area(path)), 0.0))
            continue;

        Polyline ring = path2Polygon<double>(path);
        cleanPolygon(ring);
        if (ring.size() >= 3)
            nfps.push_back(ring);
    }

    finishNfpRings(nfps, fixed, moving, false);

    qDebug() << "Minkowski NFP rings" << nfps.size();
    isExecute = true;
}

}
