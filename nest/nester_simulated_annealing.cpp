#include "nester.h"
#include "nester_common.h"

#include <QDebug>
#include <QCoreApplication>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace S_Shape2D {

void Nester::simulatedAnnealing(std::vector<Placement>& currentPlacements, int iterations)
{
    if (currentPlacements.size() < 2) return;

    struct PairNfpCacheEntry {
        std::vector<NfpGroup> groups;
        std::vector<NestPoly> flatRings;
    };

    auto pointKey = [](const NestPoint& point) {
        const auto x = static_cast<long long>(std::llround(point.x * 1000.0));
        const auto y = static_cast<long long>(std::llround(point.y * 1000.0));
        return std::to_string(x) + "," + std::to_string(y);
    };

    auto polygonKey = [](const Polyline& poly) {
        std::ostringstream stream;
        stream.setf(std::ios::fixed);
        stream.precision(3);
        stream << poly.size();
        for (const auto& pt : poly)
            stream << ';' << pt.x << ',' << pt.y;
        return stream.str();
    };

    auto translateNfpGroups = [](const std::vector<NfpGroup>& groups, const NestPoint& offset) {
        std::vector<NfpGroup> translated;
        translated.reserve(groups.size());

        for (const auto& group : groups) {
            NfpGroup copy;
            copy.outers.reserve(group.outers.size());
            copy.holes.reserve(group.holes.size());

            for (auto outer : group.outers) {
                outer.translate(offset);
                copy.outers.push_back(std::move(outer));
            }

            for (auto hole : group.holes) {
                hole.translate(offset);
                copy.holes.push_back(std::move(hole));
            }

            translated.push_back(std::move(copy));
        }

        return translated;
    };

    auto appendPairGroups = [](std::vector<NfpGroup>& target, std::vector<NfpGroup> source) {
        target.reserve(target.size() + source.size());
        for (auto& group : source)
            target.push_back(std::move(group));
    };

    auto scorePlacements = [this](const std::vector<Placement>& placementSet) {
        Box2D box = computePlacedBBox(placementSet);
        if (!box)
            return 1e18;
        return box.width() * box.height();
    };

    std::unordered_map<std::string, PairNfpCacheEntry> nfpCache;
    nfpCache.reserve(currentPlacements.size() * currentPlacements.size());
    int cacheHits = 0;
    int cacheMisses = 0;

    auto getPairNfps = [&](const Polyline& fixed, const Polyline& moving) -> const PairNfpCacheEntry& {
        const std::string key = std::to_string(config.nfpMethod) + "|" + polygonKey(fixed) + "|" + polygonKey(moving);
        auto it = nfpCache.find(key);
        if (it != nfpCache.end()) {
            cacheHits++;
            return it->second;
        }

        cacheMisses++;
        PairNfpCacheEntry entry;
        if (fixed.size() < 3 || moving.size() < 3) {
            auto inserted = nfpCache.emplace(key, std::move(entry));
            return inserted.first->second;
        }

        NfpPlacer placer(fixed, moving);
        switch (config.nfpMethod) {
        case 0:
            placer.exec();
            break;
        case 1:
            placer.execVectorSegments();
            break;
        default:
            placer.execMinkowski();
            break;
        }

        const NestPoint refOffset = moving[0];
        NfpGroup group;
        for (const auto& nfp : placer.getNFPs()) {
            if (nfp.size() < 3)
                continue;

            NestPoly corrected = nfp;
            for (auto& pt : corrected) {
                pt.x -= refOffset.x;
                pt.y -= refOffset.y;
            }

            const double area2x = signedArea2x(corrected);
            if (area2x > 0)
                group.outers.push_back(corrected);
            else if (area2x < 0)
                group.holes.push_back(corrected);
        }

        if (!group.outers.empty())
            entry.groups.push_back(std::move(group));
        entry.flatRings = flattenNfpGroups(entry.groups);

        auto inserted = nfpCache.emplace(key, std::move(entry));
        return inserted.first->second;
    };

    double currentScore = scorePlacements(currentPlacements);
    const double initialScore = currentScore;
    double bestScore = currentScore;
    auto bestPlacements = currentPlacements;

    double temp = 1000.0;
    double cooling = 0.995;
    int feasibleTrials = 0;
    int acceptedTrials = 0;
    int improvedTrials = 0;
    int lastVisualUpdate = -std::max(1, config.saUpdateInterval);
    const int updateInterval = std::max(1, config.saUpdateInterval);

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
            // Placement::polygon is already stored in its placed orientation.
            // Applying Placement::rotation here rotates it a second time and
            // makes many SA rebuilds invalid.
            Polyline poly = p.polygon;

            std::vector<NfpGroup> nfps;
            std::vector<NestPoly> flatNfps;
            for (const auto& fixedPlacement : rebuilt) {
                const PairNfpCacheEntry& cached = getPairNfps(fixedPlacement.polygon, poly);
                auto translated = translateNfpGroups(cached.groups, fixedPlacement.offset);
                appendPairGroups(nfps, std::move(translated));

                flatNfps.reserve(flatNfps.size() + cached.flatRings.size());
                for (auto ring : cached.flatRings) {
                    ring.translate(fixedPlacement.offset);
                    flatNfps.push_back(std::move(ring));
                }
            }

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

            Box2D pieceBox = calcBoundingBox(poly);
            double nfpStep = std::max(5.0, std::min(pieceBox.width(), pieceBox.height()) * 0.3);
            auto nfpSamples = sampleNfpBoundary(flatNfps, nfpStep);
            candidates.insert(candidates.end(), nfpSamples.begin(), nfpSamples.end());

            std::sort(candidates.begin(), candidates.end(), [](const NestPoint& a, const NestPoint& b) {
                if (std::fabs(a.y - b.y) > kNestEps)
                    return a.y < b.y;
                return a.x < b.x;
            });

            const int candidateLimit = std::max(1, config.saCandidateLimit);
            std::vector<NestPoint> limitedCandidates;
            limitedCandidates.reserve(std::min<int>(static_cast<int>(candidates.size()), candidateLimit));
            std::unordered_set<std::string> seenCandidates;
            for (const auto& candidate : candidates) {
                if (!seenCandidates.insert(pointKey(candidate)).second)
                    continue;

                limitedCandidates.push_back(candidate);
                if (static_cast<int>(limitedCandidates.size()) >= candidateLimit)
                    break;
            }

            NestPoint bestPos = {config.stockWidth + 1, config.stockHeight + 1};
            double bestPosScore = 1e18;
            for (int candidateIndex = 0; candidateIndex < static_cast<int>(limitedCandidates.size()); ++candidateIndex) {
                const NestPoint& cand = limitedCandidates[candidateIndex];
                const int slideLimit = std::max(0, config.saSlideCandidateLimit);

                for (int localIndex = 0; localIndex < 2; ++localIndex) {
                    if (localIndex == 1 && (!config.enableBLF || candidateIndex >= slideLimit))
                        continue;

                    const NestPoint localCand = (localIndex == 0)
                        ? cand
                        : blSlide(poly, cand, nfps, config.stockWidth, config.stockHeight);

                    if (!isInsideStock(poly, localCand)) continue;
                    if (!isOutsideAllNfps(localCand, nfps)) continue;
                    NestPoly testOverlap = poly;
                    testOverlap.translate(localCand);
                    if (overlapsAnyPlaced(testOverlap, rebuilt)) continue;
                    ScoredPosition sp = evaluatePosition(poly, localCand, p.rotation, rebuilt);
                    if (sp.score < bestPosScore) {
                        bestPosScore = sp.score;
                        bestPos = localCand;
                    }
                }
            }

            if (bestPos.x > config.stockWidth || bestPos.y > config.stockHeight) {
                ok = false;
                break;
            }
            Placement rebuiltPlacement = p;
            rebuiltPlacement.polygon = poly;
            rebuiltPlacement.offset = bestPos;
            rebuiltPlacement.rotation = p.rotation;
            rebuilt.push_back(rebuiltPlacement);
        }

        if (!ok) continue;
        feasibleTrials++;

        double trialScore = scorePlacements(rebuilt);
        double delta = trialScore - currentScore;

        if (delta < 0 || std::exp(-delta / temp) > (double)(rng() % 1000) / 1000.0) {
            currentPlacements = rebuilt;
            currentScore = trialScore;
            acceptedTrials++;
            if (currentScore < bestScore) {
                bestScore = currentScore;
                bestPlacements = currentPlacements;
                improvedTrials++;
                if (iter - lastVisualUpdate >= updateInterval) {
                    emit stepCompleted(getPlacedPolygons(), getStock(), getUtilization(), -1);
                    lastVisualUpdate = iter;
                }
            }
        }
        temp *= cooling;

        if (iter % updateInterval == 0) {
            emit saProgress(iter, iterations);
            QCoreApplication::processEvents();
        }
    }

    currentPlacements = bestPlacements;
    emit stepCompleted(getPlacedPolygons(), getStock(), getUtilization(), -1);
    emit saProgress(iterations, iterations);
    qDebug() << "SA score" << initialScore << "->" << bestScore
             << "feasible" << feasibleTrials
             << "accepted" << acceptedTrials
             << "improved" << improvedTrials
             << "cache" << cacheHits << "hits" << cacheMisses << "misses";
}

} // namespace S_Shape2D
