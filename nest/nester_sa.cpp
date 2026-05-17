#include "nester.h"
#include "nester_common.h"

#include <QDebug>
#include <QCoreApplication>
#include <cmath>

namespace S_Shape2D {

void Nester::simulatedAnnealing(std::vector<Placement>& currentPlacements, int iterations)
{
    if (currentPlacements.size() < 2) return;

    double currentScore = computePlacedBBox(currentPlacements).width() *
                          computePlacedBBox(currentPlacements).height();
    double bestScore = currentScore;
    auto bestPlacements = currentPlacements;

    double temp = 1000.0;
    double cooling = 0.995;

    for (int iter = 0; iter < iterations; ++iter) {
        int i = rng() % currentPlacements.size();
        int j = rng() % currentPlacements.size();
        if (i == j) continue;

        auto trial = currentPlacements;
        std::swap(trial[i], trial[j]);

        for (auto& p : trial) {
            p.offset = {0, 0};
        }

        std::vector<Placement> rebuilt;
        bool ok = true;
        for (auto& p : trial) {
            Polyline poly = p.polygon;
            if (std::fabs(p.rotation) > 1e-9)
                poly.rotate(p.rotation);

            auto nfps = computeNfpsForMoving(poly, rebuilt, config.nfpMethod);

            std::vector<NestPoint> candidates;
            candidates.push_back({0, 0});
            {
                Box2D bb = calcBoundingBox(poly);
                candidates.push_back({-bb.left(), -bb.top()});
            }
            for (const auto& group : nfps) {
                for (const auto& outer : group.outers)
                    for (const auto& pt : outer)
                        candidates.push_back(pt);
                for (const auto& hole : group.holes)
                    for (const auto& pt : hole)
                        candidates.push_back(pt);
            }

            NestPoint bestPos = {config.stockWidth + 1, config.stockHeight + 1};
            double bestPosScore = 1e18;
            for (auto& cand : candidates) {
                if (!isInsideStock(poly, cand)) continue;
                if (!isOutsideAllNfps(cand, nfps)) continue;
                NestPoly testOverlap = poly;
                testOverlap.translate(cand);
                if (overlapsAnyPlaced(testOverlap, rebuilt)) continue;
                ScoredPosition sp = evaluatePosition(poly, cand, p.rotation, rebuilt);
                if (sp.score < bestPosScore) {
                    bestPosScore = sp.score;
                    bestPos = cand;
                }
            }

            if (bestPos.x > config.stockWidth || bestPos.y > config.stockHeight) {
                ok = false;
                break;
            }
            rebuilt.push_back({poly, bestPos, p.rotation});
        }

        if (!ok) continue;

        double trialScore = computePlacedBBox(rebuilt).width() *
                            computePlacedBBox(rebuilt).height();
        double delta = trialScore - currentScore;

        if (delta < 0 || std::exp(-delta / temp) > (double)(rng() % 1000) / 1000.0) {
            currentPlacements = rebuilt;
            currentScore = trialScore;
            if (currentScore < bestScore) {
                bestScore = currentScore;
                bestPlacements = currentPlacements;
                emit stepCompleted(getPlacedPolygons(), getStock(), getUtilization(), -1);
            }
        }
        temp *= cooling;

        if (iter % 20 == 0) {
            emit saProgress(iter, iterations);
            QCoreApplication::processEvents();
        }
    }

    currentPlacements = bestPlacements;
}

} // namespace S_Shape2D