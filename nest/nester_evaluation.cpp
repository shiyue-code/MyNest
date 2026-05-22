#include "nester.h"
#include "nester_common.h"

#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace S_Shape2D {

Nester::ScoredPosition Nester::evaluateRotation(const Polyline& poly, double rot,
                                                const std::vector<Placement>& placed,
                                                bool useBL,
                                                bool collectPreview)
{
    NestPoly rotated = poly;
    if (std::fabs(rot) > 1e-9)
        rotated.rotate(rot);

    if (rotated.orientation() == NestPoly::Clockwise)
        rotated.reverse();

    auto nfps = computeNfpsForMoving(rotated, placed, config.nfpMethod);
    auto flatNfps = flattenNfpGroups(nfps);

    Box2D bb = calcBoundingBox(rotated);
    double pw = bb.width();
    double ph = bb.height();

    std::vector<NestPoint> candidates;
    candidates.push_back({0, 0});
    candidates.push_back({-bb.left(), -bb.top()});

    for (const auto& group : nfps) {
        for (const auto& outer : group.outers)
            for (const auto& pt : outer)
                candidates.push_back(pt);
        for (const auto& hole : group.holes)
            for (const auto& pt : hole)
                candidates.push_back(pt);
    }
    double nfpStep = std::max(5.0, std::min(pw, ph) * 0.3);
    auto nfpSamples = sampleNfpBoundary(flatNfps, nfpStep);
    candidates.insert(candidates.end(), nfpSamples.begin(), nfpSamples.end());

    ScoredPosition bestSP;
    bestSP.score = 1e18;
    bestSP.rotation = rot;

    std::vector<NestPoint> validCands;
    for (auto& cand : candidates) {
        if (!isInsideStock(rotated, cand)) continue;
        if (!isOutsideAllNfps(cand, nfps)) continue;
        NestPoly testOverlap = rotated;
        testOverlap.translate(cand);
        if (overlapsAnyPlaced(testOverlap, placed)) continue;
        validCands.push_back(cand);
    }

    if (collectPreview) {
        std::vector<Polyline> candPolys;
        candPolys.reserve(validCands.size());
        for (auto& cand : validCands) {
            Polyline p = rotated;
            p.translate(cand);
            candPolys.push_back(p);
        }

        std::vector<Polyline> nfpPolys;
        NestPoint refOff = rotated[0];
        for (auto& g : nfps) {
            for (auto& o : g.outers) {
                Polyline po = o;
                for (auto& pt : po) {
                    pt.x += refOff.x;
                    pt.y += refOff.y;
                }
                nfpPolys.push_back(po);
            }
            for (auto& h : g.holes) {
                Polyline ph = h;
                for (auto& pt : ph) {
                    pt.x += refOff.x;
                    pt.y += refOff.y;
                }
                nfpPolys.push_back(ph);
            }
        }

        bestSP.candidatePolys = std::move(candPolys);
        bestSP.nfpPolys = std::move(nfpPolys);
    }

    if (useBL) {
        std::sort(validCands.begin(), validCands.end(), [](const NestPoint& a, const NestPoint& b) {
            double da = a.y * 10000 + a.x;
            double db = b.y * 10000 + b.x;
            return da < db;
        });
        int slideLimit = std::min((int)validCands.size(), 50);
        for (int i = 0; i < slideLimit; ++i) {
            NestPoint finalPos = blSlide(rotated, validCands[i], nfps, config.stockWidth, config.stockHeight);
            if (!isInsideStock(rotated, finalPos)) continue;
            if (!isOutsideAllNfps(finalPos, nfps)) continue;
            NestPoly slideOverlap = rotated;
            slideOverlap.translate(finalPos);
            if (overlapsAnyPlaced(slideOverlap, placed)) continue;
            ScoredPosition sp = evaluatePosition(rotated, finalPos, rot, placed);
            if (sp.score < bestSP.score) {
                bestSP.pos = sp.pos;
                bestSP.rotation = sp.rotation;
                bestSP.score = sp.score;
            }
        }
    } else {
        for (auto& cand : validCands) {
            ScoredPosition sp = evaluatePosition(rotated, cand, rot, placed);
            if (sp.score < bestSP.score) {
                bestSP.pos = sp.pos;
                bestSP.rotation = sp.rotation;
                bestSP.score = sp.score;
            }
        }
    }
    return bestSP;
}

std::vector<Nester::Point> Nester::blfFill(const Polyline& poly, const std::vector<Placement>& placed) const
{
    std::vector<NestPoint> gapCandidates;

    double step = std::max(5.0, std::min(config.stockWidth, config.stockHeight) * 0.02);

    for (double y = 0; y <= config.stockHeight; y += step) {
        for (double x = 0; x <= config.stockWidth; x += step) {
            NestPoint pt = {x, y};
            if (!isInsideStock(poly, pt)) continue;

            bool inGap = true;
            for (const auto& p : placed) {
                NestPoly fixed = p.polygon;
                fixed.translate(p.offset);
                ClipperLib::IntPoint cp;
                cp.X = static_cast<ClipperLib::cInt>(std::llround(pt.x * clipperScaler));
                cp.Y = static_cast<ClipperLib::cInt>(std::llround(pt.y * clipperScaler));
                ClipperLib::Path path = polygon2Path(fixed);
                if (ClipperLib::PointInPolygon(cp, path) != 0) {
                    inGap = false;
                    break;
                }
            }
            if (inGap)
                gapCandidates.push_back(pt);
        }
    }
    return gapCandidates;
}

} // namespace S_Shape2D
