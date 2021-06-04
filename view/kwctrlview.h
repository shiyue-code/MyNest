#ifndef KWCTRLVIEW_H
#define KWCTRLVIEW_H

#include <QPainterPath>
#include <QWidget>

#ifndef ARM
#include <QOpenGLExtraFunctions>
#include <QOpenGLWidget>

#define TEXTURE_NUM 2

class KWCtrlView
    : public QOpenGLWidget,
      protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit KWCtrlView(QWidget* parent = nullptr);
    ~KWCtrlView() override;

    virtual void ResetView();

    void SetCoordMirrorX(bool bMirror);
    void SetCoordMirrorY(bool bMirror);

    ///其他
    void ViewPoint(const QPointF& pt);
    void ScrPint(const QPointF& pt);
    void glColor(const QColor& clr);

    ///坐标转换
    void Scr2View(QPointF& pt);
    void View2Scr(QPointF& pt);

    bool IsTextureVisiable(int i);
    void SetTextureVisiable(int i, bool isVisable);
    void SetTextureOffset(int i, double x = 0, double y = 0);

