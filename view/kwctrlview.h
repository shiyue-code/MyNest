#ifndef KWCTRLVIEW_H
#define KWCTRLVIEW_H

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPainter>
#include <QWidget>

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

    void ViewPoint(const QPointF& pt);
    void ScrPint(const QPointF& pt);
    void glColor(const QColor& clr);

    void Scr2View(QPointF& pt);
    void View2Scr(QPointF& pt);

    bool IsTextureVisiable(int i);
    void SetTextureVisiable(int i, bool isVisable);
    void SetTextureOffset(int i, double x = 0, double y = 0);

protected:
    virtual void DrawGLSence(QPainter&) { }
    virtual void MouseLButtonPressed(QPointF&, QPointF&) { }
    virtual void MouseLButtonReleased(QPointF&, QPointF&) { }
    virtual void MouseRButtonPressed(QPointF&, QPointF&) { }
    virtual void MouseRButtonReleased(QPointF&, QPointF&) { }
    virtual void MouseMove(QPointF&, QPointF&) { update(); }
    virtual void MouseLDoubleClick(QPointF&, QPointF&) { }
    virtual void MouseRDoubleClick(QPointF&, QPointF&) { }
    virtual void Initialize() { }

    void DrawTick(QPainter& painter);
    void DrawOrigin(QPainter& painter);
    void DrawCross();

    void ScaleAndTrans(QPointF& pt);
    void Scale(QPointF& pt);
    void Trans(QPointF& pt);
    void DrawArrow(const QPointF& ptStart, const QPointF& ptEnd,
                   bool bBoth = false, int arrowsize = 10, float lineW = 1);

    void DrawSelectBox();

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;

    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    void enterEvent(QEvent* evt) override;
    void leaveEvent(QEvent* evt) override;

    void BindTexture(const QImage& pix, int i);
    void DrawTexture(int i);

protected:
    bool m_bShowTick = true;
    bool m_bshowOrigin = true;
    bool m_bShowCross = true;
    bool m_bShowSelBox = true;
    bool m_bCoordMirrorY = false;
    bool m_bCoordMirrorX = false;

    bool m_bActiveMouse = false;
    bool m_bLButtonDown = false;
    bool m_bRButtonDown = false;

    float m_fScale = 1.0f;
    float m_fZoomStep = 0.25f;
    float m_fLineWidth = 1.0f;
    float m_fPointSize = 1.5f;

    double m_dXTrans = 0.0;
    double m_dXTransOld = 0.0;
    double m_dYTrans = 0.0;
    double m_dYTransOld = 0.0;
    double m_dViewH = 300.0;
    double m_dViewW = 300.0;

    GLuint m_texture[TEXTURE_NUM] = { 0, 0 };
    bool m_bTextureVisable[TEXTURE_NUM] { false, false };
    QImage m_imgTexture[TEXTURE_NUM];
    QPointF m_ptTextureOffset[TEXTURE_NUM];

    QRectF m_rect { -100, 100, -100, 100 };
    QRectF m_rectSelectingBox;
    QPointF m_ptOrigin = QPointF(0.0, 0.0);
    QPointF m_ptStart;
    QPointF m_ptEnd;
    QPointF m_ptCurCursor;

    QColor m_clrTick { "#cccccc" };
    QColor m_clrBackGround { "#000000" };
    QFont m_fontSong = QFont("宋体", 11, QFont::DemiBold);
    QPixmap m_painterPix;
};

#endif // KWCTRLVIEW_H
