#include "nfpplacer_common.h"

#include <QDebug>
#include <QElapsedTimer>

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

    QElapsedTimer phaseTimer;
    phaseTimer.start();
    std::vector<Segment> candidates = generateCandidateSegments(fixed, moving);
    const qint64 candidateMs = phaseTimer.elapsed();
    phaseTimer.restart();
    std::vector<Segment> split = splitSegments(candidates);
    const qint64 splitMs = phaseTimer.elapsed();
    phaseTimer.restart();
    qDebug() << "Vector segment NFP"
             << "fixedOrientation" << fixed.orientation()
             << "movingOrientation" << moving.orientation()
             << "candidates" << candidates.size()
             << "split" << split.size();

    nfps = extractRings(split);

    qDebug() << "Vector segment NFP rings" << nfps.size();
    if (nfps.empty())
        qDebug() << "Vector segment extraction produced no closed NFP ring.";

    const qint64 ringMs = phaseTimer.elapsed();
    phaseTimer.restart();
    finishNfpRings(nfps, fixed, moving, false, false);
    const qint64 finishMs = phaseTimer.elapsed();
    qDebug() << "Vector segment NFP phases"
             << "candidate" << candidateMs << "ms"
             << "split" << splitMs << "ms"
             << "rings" << ringMs << "ms"
             << "finish" << finishMs << "ms";
    isExecute = true;
}

}
