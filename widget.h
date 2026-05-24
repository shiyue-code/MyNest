#ifndef WIDGET_H
#define WIDGET_H

#include <QTimer>
#include <QThread>
#include <QWidget>
#include <QPointer>
#include <map>
#include <vector>

#include "nest/nest_scene.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class QLabel;
class MyCtrlView;
class NestCanvasView;
class QProgressBar;
class QStackedWidget;
class QVBoxLayout;
class QTableWidget;
class QWidget;
namespace S_Shape2D { class Nester; }

class Widget : public QWidget {
    Q_OBJECT

public:
    Widget(QWidget* parent = nullptr);
    ~Widget();

private slots:
    void onDrawP1();
    void onDrawP2();

    void onExec();
    void onNest();
    void onAddShape();
    void onGenerateRandomShapes();
    void onRemoveShape();
    void onClearShapes();

    void onTimer();
    void onThemeChanged(int index);

    void OnSave();
    void OnLoad(const QString& absoluteFilePath = QString());

private:
    using Polyline = S_Shape2D::Polyline2D;

    void addShapePrototype(const QString& name, const Polyline& polygon, int quantity);
    void refreshShapeTable();
    void syncShapeLibraryFromTable();
    S_Shape2D::NestScene buildNestSceneFromUi();
    Polyline selectedSourcePolygon(QString* sourceName = nullptr);
    QColor colorForShape(int index) const;
    void setupModernInterface();
    void syncPreviewViews();
    void updateNestSummaryTable();
    void resetNestProgress(int totalPieces, int saIterations);
    void setNestPhaseText(const QString& text);
    void updateStatusCards(int totalPieces = -1, int placedPieces = -1, double utilization = -1.0);

    Ui::Widget* ui;

    QTimer timer;
    QThread* nestThread = nullptr;
    S_Shape2D::Nester* nester = nullptr;
    std::vector<S_Shape2D::ShapePrototype> shapeLibrary;
    S_Shape2D::NestScene currentNestScene;
    std::map<int, int> placedPrototypeCounts;
    int nextPrototypeId = 1;
    int nestTotalPieces = 0;
    int nestPlacedCount = 0;
    int nestProcessedPieces = 0;
    int nestSAIterations = 0;
    enum NestPhase { NestPlacing, NestBLF, NestSA } currentNestPhase = NestPlacing;
    QLabel* lblStatusShapes = nullptr;
    QLabel* lblStatusTotal = nullptr;
    QLabel* lblStatusPlaced = nullptr;
    QLabel* lblStatusUnplaced = nullptr;
    QLabel* lblStatusUtilization = nullptr;
    MyCtrlView* fixedPreviewView = nullptr;
    MyCtrlView* movingPreviewView = nullptr;
    MyCtrlView* nestDrawView = nullptr;
    NestCanvasView* nestCanvasView = nullptr;
    QStackedWidget* workbenchCanvasStack = nullptr;
    QTableWidget* tblNestSummary = nullptr;
    QWidget* shapeCardsContainer = nullptr;
    QVBoxLayout* shapeCardsLayout = nullptr;
    QWidget* resultCardsContainer = nullptr;
    QVBoxLayout* resultCardsLayout = nullptr;
    QProgressBar* nestProgressBar = nullptr;
    QLabel* lblNestPhase = nullptr;
    bool syncingPreviewViews = false;
};
#endif // WIDGET_H
