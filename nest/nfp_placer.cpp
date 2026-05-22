#include "nfp_placer.h"

namespace S_Shape2D {

NfpPlacer::NfpPlacer(NfpPlacer::CPolylineRef fixed, NfpPlacer::CPolylineRef moving)
    : fixedPolygon(fixed)
    , movingPolygon(moving)
{
}

void NfpPlacer::set(NfpPlacer::CPolylineRef fixed, NfpPlacer::CPolylineRef moving)
{
    fixedPolygon = fixed;
    movingPolygon = moving;

    nfps.clear();
    isExecute = false;
}

NfpPlacer::Container NfpPlacer::getNFPs()
{
    return nfps;
}

}
