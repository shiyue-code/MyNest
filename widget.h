#ifndef WIDGET_H
#define WIDGET_H

#include <QTimer>
#include <QThread>
#include <QWidget>
#include <QPointer>
#include <vector>

#include "nest/nest_scene.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class NestWindow;
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

    Ui::Widget* ui;

    QTimer timer;
    QPointer<NestWindow> nestWindow;
    QThread* nestThread = nullptr;
    S_Shape2D::Nester* nester = nullptr;
    std::vector<S_Shape2D::ShapePrototype> shapeLibrary;
    int nextPrototypeId = 1;
};
#endif // WIDGET_H
