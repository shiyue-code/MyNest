#ifndef WIDGET_H
#define WIDGET_H

#include <QTimer>
#include <QThread>
#include <QWidget>
#include <QPointer>

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

    void onTimer();

    void OnSave();
    void OnLoad(const QString &absoluteFilePath = QString::Null());

private:
    Ui::Widget* ui;

    QTimer timer;
    QPointer<NestWindow> nestWindow;
    QThread* nestThread = nullptr;
    S_Shape2D::Nester* nester = nullptr;
};
#endif // WIDGET_H
