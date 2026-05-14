#ifndef NESTWINDOW_H
#define NESTWINDOW_H

#include <QDialog>
#include <QLabel>
#include <vector>
#include "shapes/s_polyline.hpp"

class KWCtrlView;

class NestWindow : public QDialog {
    Q_OBJECT
public:
    using Polyline = S_Polyline2D;

    explicit NestWindow(QWidget* parent = nullptr);

    void setNestResult(const Polyline& stock,
                       const std::vector<Polyline>& placed,
                       double utilization);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    class NestView;
    NestView* view;
    QLabel* lblInfo;
};

#endif // NESTWINDOW_H
