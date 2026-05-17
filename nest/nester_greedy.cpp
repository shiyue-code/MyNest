#include "nester.h"
#include "nester_common.h"

#include <QDebug>
#include <QElapsedTimer>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace S_Shape2D {

void Nester::execGreedy()
{
    QElapsedTimer timer;
    timer.start();
    placements.clear();

    std::vector<int> indices(polygons.size());
    for (size_t i = 0; i < indices.size(); ++i)
        indices[i] = i;
    sortByComplexity(indices);

    qDebug() << "Greedy sorted:" << indices << "rotation steps:" << config.rotationSteps;

    for (int idx : indices) {
        Polyline poly = polygons[idx];
        cleanPolygon(poly);
        if (poly.size() < 3) continue;
        if (poly.orientation() == Polyline::Clockwise)
            poly.reverse();

        auto rotations = getRotationAngles();

        ScoredPosition bestSP;
        bestSP.score = 1e18;
        bool found = false;

        for (double rot : rotations) {
            ScoredPosition sp = evaluateRotation(poly, rot, placements, false);

            if (!sp.candidatePolys.empty()) {
                Polyline rotated = poly;
                if (std::fabs(rot) > 1e-9)
                    rotated.rotate(rot);
                emit candidatesReady(sp.candidatePolys, sp.nfpPolys, rotated);
            }

            if (sp.score < bestSP.score) {
                bestSP = sp;
                found = true;
            }
        }

        if (found) {
            Polyline placedPoly = poly;
            if (std::fabs(bestSP.rotation) > 1e-9)
                placedPoly.rotate(bestSP.rotation);
            placements.push_back({placedPoly, bestSP.pos, bestSP.rotation});

            double util = getUtilization();
            qDebug() << "Greedy placed" << idx << "at" << bestSP.pos.x << bestSP.pos.y
                     << "rot" << (bestSP.rotation * 180 / M_PI)
                     << "| util:" << util << "%";
            emit stepCompleted(getPlacedPolygons(), getStock(), getUtilization(), idx);
        } else {
            qDebug() << "Greedy skipped" << idx;
        }
    }

    if (config.enableBLF) {
        qDebug() << "Greedy BLF gap filling...";
        emit phaseChanged(QString::fromWCharArray(L"BLF\u586B\u5145\u4E2D..."));
        for (int idx : indices) {
            Polyline poly = polygons[idx];
            cleanPolygon(poly);
            if (poly.size() < 3) continue;
            if (poly.orientation() == Polyline::Clockwise)
                poly.reverse();

            bool inPlaced = false;
            for (size_t pi = 0; pi < placements.size(); ++pi) {
                if (std::fabs(placements[pi].polygon.area() - poly.area()) < 1e-6 &&
                    placements[pi].polygon.size() == poly.size()) {
                    inPlaced = true;
                    break;
                }
            }
            if (inPlaced) continue;

            auto gapCands = blfFill(poly, placements);
            ScoredPosition bestGap;
            bestGap.score = 1e18;
            bool gapFound = false;

            for (auto& cand : gapCands) {
                for (double rot : {0.0, M_PI / 2, M_PI, 3 * M_PI / 2}) {
                    NestPoly rotated = poly;
                    if (std::fabs(rot) > 1e-9)
                        rotated.rotate(rot);
                    if (!isInsideStock(rotated, cand)) continue;

                    auto nfps = computeNfpsForMoving(rotated, placements, config.nfpMethod);
                    if (!isOutsideAllNfps(cand, nfps)) continue;

                    NestPoly testOverlap = rotated;
                    testOverlap.translate(cand);
                    if (overlapsAnyPlaced(testOverlap, placements)) continue;

                    ScoredPosition sp = evaluatePosition(rotated, cand, rot, placements);
                    if (sp.score < bestGap.score) {
                        bestGap = sp;
                        gapFound = true;
                    }
                }
            }

            if (gapFound) {
                Polyline placedPoly = poly;
                if (std::fabs(bestGap.rotation) > 1e-9)
                    placedPoly.rotate(bestGap.rotation);
                placements.push_back({placedPoly, bestGap.pos, bestGap.rotation});
                qDebug() << "Greedy BLF filled" << idx << "at" << bestGap.pos.x << bestGap.pos.y;
                emit stepCompleted(getPlacedPolygons(), getStock(), getUtilization(), idx);
            }
        }
    }

    if (config.enableSA && placements.size() >= 2) {
        qDebug() << "Simulated annealing optimization...";
        emit phaseChanged(QString::fromWCharArray(L"SA\u4F18\u5316\u4E2D..."));
        simulatedAnnealing(placements, config.saIterations);
        qDebug() << "SA done, util:" << getUtilization() << "%";
    }

    qDebug() << "Greedy took" << timer.elapsed() << "ms placed" << placements.size() << "/" << polygons.size();
}

} // namespace S_Shape2D