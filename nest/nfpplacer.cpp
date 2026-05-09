#include "nfpplacer.h"

namespace S_Shape2D {

NfpPlacer::NfpPlacer(NfpPlacer::CPolylineRef p1, NfpPlacer::CPolylineRef p2)
    : polyA(p1)
    , polyB(p2)
{
}

void NfpPlacer::set(NfpPlacer::CPolylineRef A, NfpPlacer::CPolylineRef B)
{
    polyA = A;
    polyB = B;

    nfps.clear();
    isExecute = false;
}

NfpPlacer::Container NfpPlacer::getNFPs()
{
    return nfps;
}

}
