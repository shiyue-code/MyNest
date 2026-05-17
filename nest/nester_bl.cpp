#include "nester.h"
#include "nester_common.h"

#include <QDebug>
#include <QElapsedTimer>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace S_Shape2D {

void Nester::execBL()
{
    QElapsedTimer timer;
    timer.start();
    placements.clear();

    std::vector<int> indices(polygons.size());
    for (size_t i = 0; i < indices.size(); ++i)
        indices[i] = i;
    sortByComplexity(indices);

    qDebug() << "BL sorted:" << indices << "rotation steps:" << config.rotationSteps;

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
            ScoredPosition sp = evaluateRotation(poly, rot, placements, true);

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
            qDebug() << "BL placed" << idx << "at" << bestSP.pos.x << bestSP.pos.y
                     << "rot" << (bestSP.rotation * 180 / M_PI)
                     << "| util:" << util << "%";
            emit stepCompleted(getPlacedPolygons(), getStock(), getUtilization(), idx);
        } else {
            qDebug() << "BL skipped" << idx;
        }
    }

    if (config.enableBLF) {
        qDebug() << "BLF gap filling...";
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
                qDebug() << "BLF filled" << idx << "at" << bestGap.pos.x << bestGap.pos.y;
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

    qDebug() << "BL took" << timer.elapsed() << "ms placed" << placements.size() << "/" << polygons.size();
}

bool Nester::backtrackPlace(std::vector<int>& order, int depth, std::vector<Placement>& result,
                            bool useBL)
{
    if (depth >= (int)order.size())
        return true;

    Polyline poly = polygons[order[depth]];
    cleanPolygon(poly);
    if (poly.size() < 3)
        return backtrackPlace(order, depth + 1, result, useBL);
    if (poly.orientation() == Polyline::Clockwise)
        poly.reverse();

    auto rotations = getRotationAngles();
    ScoredPosition bestSP;
    bestSP.score = 1e18;
    bool found = false;

    for (double rot : rotations) {
        NestPoly rotated = poly;
        if (std::fabs(rot) > 1e-9)
            rotated.rotate(rot);

        auto nfps = computeNfpsForMoving(rotated, result, config.nfpMethod);
        auto flatNfps = flattenNfpGroups(nfps);

        std::vector<NestPoint> candidates;
        candidates.push_back({0, 0});
        {
            Box2D bb = calcBoundingBox(rotated);
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

        Box2D bb = calcBoundingBox(rotated);
        double nfpStep = std::max(5.0, std::min(bb.width(), bb.height()) * 0.3);
        auto nfpSamples = sampleNfpBoundary(flatNfps, nfpStep);
        candidates.insert(candidates.end(), nfpSamples.begin(), nfpSamples.end());

        for (auto& cand : candidates) {
            if (!isInsideStock(rotated, cand)) continue;
            if (!isOutsideAllNfps(cand, nfps)) continue;
            NestPoly testOverlap = rotated;
            testOverlap.translate(cand);
            if (overlapsAnyPlaced(testOverlap, result)) continue;

            ScoredPosition sp;
            if (useBL) {
                NestPoint finalPos = blSlide(rotated, cand, nfps, config.stockWidth, config.stockHeight);
                if (!isInsideStock(rotated, finalPos)) continue;
                if (!isOutsideAllNfps(finalPos, nfps)) continue;
                NestPoly slideOverlap = rotated;
                slideOverlap.translate(finalPos);
                if (overlapsAnyPlaced(slideOverlap, result)) continue;
                sp = evaluatePosition(rotated, finalPos, rot, result);
            } else {
                sp = evaluatePosition(rotated, cand, rot, result);
            }

            if (sp.score < bestSP.score) {
                bestSP = sp;
                bestSP.rotation = rot;
                found = true;
            }
        }
    }

    if (!found) {
        if (depth < config.backtrackDepth && depth > 0) {
            result.pop_back();
            return backtrackPlace(order, depth - 1, result, useBL);
        }
        return false;
    }

    Polyline placedPoly = poly;
    if (std::fabs(bestSP.rotation) > 1e-9)
        placedPoly.rotate(bestSP.rotation);
    result.push_back({placedPoly, bestSP.pos, bestSP.rotation});

    return backtrackPlace(order, depth + 1, result, useBL);
}

} // namespace S_Shape2D