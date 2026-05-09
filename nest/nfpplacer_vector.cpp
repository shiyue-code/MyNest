#include "nfpplacer_common.h"

#include <QDebug>

namespace S_Shape2D {

using namespace NfpDetail;

void NfpPlacer::execVectorSegments()
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

    std::vector<Segment> candidates = generateCandidateSegments(fixed, moving);
    std::vector<Segment> split = splitSegments(candidates);
    std::vector<Segment> boundary = filterBoundarySegments(split, fixed, moving);
    qDebug() << "Vector segment NFP"
             << "fixedOrientation" << fixed.orientation()
             << "movingOrientation" << moving.orientation()
             << "candidates" << candidates.size()
             << "split" << split.size()
             << "boundary" << boundary.size();

    nfps = extractRings(boundary);
    if (nfps.empty() && boundary.size() != split.size()) {
        qDebug() << "Boundary segments did not close; retrying with split vector segment set.";
        nfps = extractRings(split);
    }

    qDebug() << "Vector segment NFP rings" << nfps.size();
    if (nfps.empty())
        qDebug() << "Vector segment extraction produced no closed NFP ring.";

    finishNfpRings(nfps, fixed, moving, true);
    isExecute = true;
}

}
