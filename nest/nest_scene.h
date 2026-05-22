#ifndef NEST_SCENE_H
#define NEST_SCENE_H

#include "shapes/s_polyline.hpp"

#include <QColor>
#include <QString>

#include <algorithm>
#include <vector>

namespace S_Shape2D {

struct ShapePrototype {
    int id = -1;
    QString name;
    Polyline2D contour;
    QColor color = QColor("#4C78A8");
    int quantity = 1;
    bool enabled = true;
    std::vector<double> allowedRotations;
};

struct PieceInstance {
    int id = -1;
    int prototypeId = -1;
    double rotation = 0.0;
    bool placed = false;
};

struct ScenePlacement {
    int pieceId = -1;
    int prototypeId = -1;
    Polyline2D polygon;
    Point2D position;
    double rotation = 0.0;
};

struct NestScene {
    double stockWidth = 500.0;
    double stockHeight = 500.0;
    std::vector<ShapePrototype> prototypes;
    std::vector<PieceInstance> pieces;
    std::vector<ScenePlacement> placements;
    int activePieceId = -1;

    int totalQuantity() const
    {
        int total = 0;
        for (const auto& prototype : prototypes) {
            if (prototype.enabled && prototype.contour.size() >= 3)
                total += std::max(0, prototype.quantity);
        }
        return total;
    }
};

inline const ShapePrototype* findPrototype(const NestScene& scene, int prototypeId)
{
    for (const auto& prototype : scene.prototypes) {
        if (prototype.id == prototypeId)
            return &prototype;
    }
    return nullptr;
}

inline std::vector<PieceInstance> expandScenePieces(const NestScene& scene)
{
    std::vector<PieceInstance> pieces;
    int nextPieceId = 0;
    for (const auto& prototype : scene.prototypes) {
        if (!prototype.enabled || prototype.contour.size() < 3)
            continue;

        for (int i = 0; i < prototype.quantity; ++i) {
            pieces.push_back({nextPieceId++, prototype.id, 0.0, false});
        }
    }
    return pieces;
}

} // namespace S_Shape2D

#endif // NEST_SCENE_H
