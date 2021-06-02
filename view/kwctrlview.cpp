#include "kwctrlview.h"

#include <QMouseEvent>
#include <QDebug>
#include <QPainter>

#include <ctime>
#include <cmath>

# ifndef ARM
#include <gl/GLU.h>

using namespace  std;

KWCtrlView::KWCtrlView(QWidget *parent)
    : QOpenGLWidget(parent)
    , QOpenGLFunctions()
{
}

KWCtrlView::~KWCtrlView()
{
}

void KWCtrlView::ResetView()
{
    if(!isValid())
        return;
    int deltaX = m_bCoordMirrorX ? -1 : 1;
    int deltaY = m_bCoordMirrorY ? -1 : 1;

    makeCurrent();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-m_dViewW * deltaX, m_dViewW * deltaX, -m_dViewH * deltaY, m_dViewH * deltaY);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glViewport(0, 0, width(), height());

    update();
}

void KWCtrlView::SetCoordMirrorX(bool bMirror)
{
    m_bCoordMirrorX = bMirror;
    ResetView();
}

void KWCtrlView::SetCoordMirrorY(bool bMirror)
{
    m_bCoordMirrorY = bMirror;
    ResetView();
}

void KWCtrlView::DrawTick(QPainter &painter)
{
    if(!m_bShowTick)
        return;

    glColor(m_clrTick);

    int deltaX = m_bCoordMirrorX ? -1 : 1;
    int deltaY = m_bCoordMirrorY ? -1 : 1;
    int x, y;
    static QString str;
    QPointF tPt;

    QRect rect = this->rect();

    static QPointF ptLeftTop, ptLeftBot, ptRightTop;
    ptLeftTop.rx() = rect.left();
    ptLeftTop.ry() = rect.top();
    ptLeftBot.rx() = rect.left();
    ptLeftBot.ry() = rect.bottom();
    ptRightTop.rx() = rect.right();
    ptRightTop.ry() = rect.top();
    Scr2View(ptLeftTop);
    Scr2View(ptLeftBot);
    Scr2View(ptRightTop);

    double delta = fabs((min(fabs(ptLeftBot.y() - ptRightTop.y()), fabs(ptRightTop.x() - ptLeftBot.x() ))) / 50.0 );
    double delta2 = delta * 1.5;

    int pix = 5;

    int stepx = 1;
    int stepy = 1;

    int modx = 5, mody = 5;
    double height = fabs(ptLeftTop.y() - ptLeftBot.y());
    if(height > 1 && rect.height() > 1)
    {
        stepy = round(pix / (rect.height() / height));
        stepy = stepy == 0 ? 1 : stepy;
    }
    else
    {
        stepy = 1;
    }

    stepy *= deltaY;
    mody = 5 * stepy;
    int ys = int(ptLeftBot.y());
    ys = (ys / mody ) * mody - mody;

    pix = 10;
    double width = fabs(ptLeftTop.x() - ptRightTop.x());
    if(width > 1 && rect.width() > 1)
    {
        stepx = round(pix / (rect.width() / width));
        stepx = stepx == 0 ? 1 : stepx;
    }
    else
    {
        stepx = 1;
    }
    stepx *= deltaX;
    modx = 5 * stepx;

    int xs = int(ptLeftTop.x());
    xs = (xs / modx) * modx - modx;

    int ye = int(ptLeftTop.y());
    int xe = int(ptRightTop.x());

    //! 竖直画标线
    glBegin(GL_LINES);
    for (y = ys ; (m_bCoordMirrorY ?  y >= ye : y <= ye); y += stepy)
    {
        glVertex2d(ptLeftTop.x(), y);
        if (y % mody == 0)
            glVertex2d(ptLeftTop.x() + delta2 * deltaX, y);
        else
            glVertex2d(ptLeftTop.x() + delta * deltaX, y);

        glVertex2d(ptRightTop.x(), y);
        if (y % mody == 0)
            glVertex2d(ptRightTop.x() - delta2 * deltaX , y);
        else
            glVertex2d(ptRightTop.x() - delta * deltaX, y);
    }

    ///画水平刻度
    for (x = xs ; (m_bCoordMirrorX ? x >= xe : x <= xe); x += stepx)
    {
        glVertex2d(x, ptLeftTop.y());
        if (x % modx == 0)
            glVertex2d(x, ptLeftTop.y() - delta2 * deltaY);
        else
            glVertex2d(x, ptLeftTop.y() - delta * deltaY);

        glVertex2d(x, ptLeftBot.y());
        if (x % modx == 0)
            glVertex2d(x, ptLeftBot.y() + delta2 * deltaY);
        else
            glVertex2d(x, ptLeftBot.y() + delta * deltaY);
    }
    glEnd();

    //!左下角箭头
    double deltaDir = delta * 2;
    double deltaDir2 = deltaDir * 2;
    double deltaDir3 = deltaDir * 3;
    glBegin(GL_LINES);
    glColor3f(1.0f, 0.2f, 0.0f);
    glVertex2d(ptLeftBot.x() + deltaDir * deltaX, ptLeftBot.y() + deltaDir2 * deltaY);
    glVertex2d(ptLeftBot.x() + deltaDir3 * deltaX, ptLeftBot.y() + deltaDir2 * deltaY);

    glColor3f(0.0f, 0.2f, 1.0f);
    glVertex2d(ptLeftBot.x() + deltaDir2 * deltaX, ptLeftBot.y() + deltaDir * deltaY);
    glVertex2d(ptLeftBot.x() + deltaDir2 * deltaX, ptLeftBot.y() + deltaDir3 * deltaY);
    glEnd();

    double e = deltaDir / 2;
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.2f, 0.0f);
    glVertex2d(ptLeftBot.x() + deltaDir2 * deltaX + deltaDir + e, ptLeftBot.y() + deltaDir2 * deltaY);
    glVertex2d(ptLeftBot.x() + deltaDir2 * deltaX + deltaDir, ptLeftBot.y() + (deltaDir2 + e / sqrt(3.0))* deltaY);
    glVertex2d(ptLeftBot.x() + deltaDir2 * deltaX + deltaDir, ptLeftBot.y() + (deltaDir2 - e / sqrt(3.0))* deltaY);

    glColor3f(0.0f, 0.2f, 1.0f);
    glVertex2d(ptLeftBot.x() + deltaDir2 * deltaX, ptLeftBot.y() + deltaDir2 * deltaY + deltaDir + e);
    glVertex2d(ptLeftBot.x() + (deltaDir2 + e / sqrt(3.0))* deltaX, ptLeftBot.y() + deltaDir2 * deltaY + deltaDir);
    glVertex2d(ptLeftBot.x() + (deltaDir2 - e / sqrt(3.0)) * deltaX, ptLeftBot.y() + deltaDir2 * deltaY + deltaDir);
    glEnd();

    painter.save();
    painter.setPen(m_clrTick);

    ///画竖直刻度数字
    for (y = ys + mody ; (m_bCoordMirrorY ?  y >= ye : y <= ye); y += stepy)
    {
        if (y % mody == 0)
        {
            str.setNum(y);
            tPt.rx() = ptLeftTop.x() + delta2 * deltaX;
            tPt.ry() = y;
            View2Scr(tPt);
            painter.drawText(tPt.x(), tPt.y(), str);
        }
    }
    for (x = xs + modx; (m_bCoordMirrorX ? x >= xe : x <= xe); x += stepx)
    {
        if (x % modx == 0)
        {
            str.setNum(x);
            tPt.rx() = x;
            tPt.ry() = ptLeftBot.y() + delta2 * deltaY;
            View2Scr(tPt);
            painter.drawText(tPt.x(), tPt.y(), str);
        }
    }
    painter.restore();
}

void KWCtrlView::DrawOrigin(QPainter &painter)
{
    Q_UNUSED(painter)
    if(!m_bshowOrigin)
        return;
    double nRatio = 20 * m_fScale;
    double dL = max(m_dViewH, m_dViewW);
    glDisable(GL_LINE_STIPPLE);

    glColor3f(0.0f, 0.2f, 1.0f);
    glBegin(GL_LINES);
    glVertex2d(m_ptOrigin.x(), m_ptOrigin.y() - dL / nRatio);
    glVertex2d(m_ptOrigin.x(), m_ptOrigin.y() + dL / nRatio);
    glEnd();

    glColor3f(1.0f, 0.2f, 0.0f);
    glBegin(GL_LINES);
    glVertex2d(m_ptOrigin.x() - dL / nRatio, m_ptOrigin.y());
    glVertex2d(m_ptOrigin.x() + dL / nRatio, m_ptOrigin.y());
    glEnd();
}

void KWCtrlView::DrawCross()
{
    if(!m_bShowCross || m_bLButtonDown || m_bRButtonDown || !m_bActiveMouse)
        return;
    glColor3f(0.9f, 0.9f, 0.8f);
    glBegin(GL_LINES);
    glVertex2d(m_ptCurCursor.x() - INT_MAX, m_ptCurCursor.y());
    glVertex2d(m_ptCurCursor.x() + INT_MAX, m_ptCurCursor.y());
    glVertex2d(m_ptCurCursor.x(), m_ptCurCursor.y() - INT_MAX);
    glVertex2d(m_ptCurCursor.x(), m_ptCurCursor.y() + INT_MAX);
    glEnd();
}

void KWCtrlView::Scr2View(QPointF &pt)
{
    QRect ClientRect = this->rect();		// 获取视口区域大小

    float w = ClientRect.width();		// 窗口宽度 w
    float h = ClientRect.height();		// 窗口高度 h
    float aspect = w / h;

    if (h == 0)
    {
        aspect = w;
    }

    float centex = w / 2;		// 中心位置
    float centey = h / 2;		// 中心位置

    int deltaX = m_bCoordMirrorX ? -1 : 1;
    int deltaY = m_bCoordMirrorY ? -1 : 1;

    // 屏幕的视觉宽度为 m_dViewHeight * aspect
    float tmpx = deltaX * 2 * m_dViewH * aspect * (pt.x() - centex) / w;	// 屏幕上点坐标转化为OpenGL画图的规范坐标
    float tmpy = deltaY * 2 * m_dViewH * (centey - pt.y()) / h;
    pt = QPointF(tmpx, tmpy);

    //偏移缩放
    ScaleAndTrans(pt);
}

void KWCtrlView::View2Scr(QPointF &pt)
{
    pt.rx() += m_dXTrans;
    pt.rx() *= m_fScale;
    pt.ry() += m_dYTrans;
    pt.ry() *= m_fScale;
    QRect ClientRect = this->rect();		// 获取视口区域大小

    int w = ClientRect.width();		// 窗口宽度 w
    int h = ClientRect.height();		// 窗口高度 h
    double aspect = (double)w / h;

    if (h == 0)
    {
        aspect = w;
    }

    double centex = w / 2;		// 中心位置
    double centey = h / 2;		// 中心位置

    // 屏幕的视觉宽度为 m_dViewHeight * aspect
    double tmpx, tmpy;
    int deltaX = m_bCoordMirrorX ? -1 : 1;
    int deltaY = m_bCoordMirrorY ? -1 : 1;

    tmpx = deltaX * pt.x()*w / 2 / m_dViewH / aspect + centex;//OpenGL画图的规范坐标转化为屏幕上点坐标
    tmpy = centey - deltaY * pt.y()*h / 2 / m_dViewH;
    pt = QPointF(tmpx, tmpy);
}

void KWCtrlView::ScaleAndTrans(QPointF &pt)
{
    Scale(pt);
    Trans(pt);
}

void KWCtrlView::Scale(QPointF &pt)
{
    pt.rx() /= m_fScale;
    pt.ry() /= m_fScale;

}

void KWCtrlView::Trans(QPointF &pt)
{
    pt.rx() -= m_dXTrans;
    pt.ry() -= m_dYTrans;
}

void KWCtrlView::DrawArrow(const QPointF &ptStart, const QPointF &ptEnd, bool bBoth, int arrowsize, float lineW)
{
    glLineWidth(lineW);

    glBegin(GL_LINES);
    ViewPoint(ptStart);
    ViewPoint(ptEnd);
    glEnd();

    QRect rect = this->rect();
    QPointF ptLeftTop, ptRightTop;
    ptLeftTop.rx() = rect.left();
    ptLeftTop.ry() = rect.top();
    ptRightTop.rx() = rect.right();
    ptRightTop.ry() = rect.top();
    Scr2View(ptLeftTop);
    Scr2View(ptRightTop);

    QVector2D dir, dirT;
    QPointF ptTmp, pt0, pt1;

    ///箭头大小设置
    double L = abs(ptRightTop.x() - ptLeftTop.x()) / (rect.width() / arrowsize);
    double W = L / sqrt(3);

    dir = QVector2D(ptStart - ptEnd);
    dir.normalize();
    ptTmp = ptEnd + L * dir.toPointF();

    dirT = QVector2D(-dir.y(), dir.x());
    pt0 = ptTmp + W * dirT.toPointF();
    pt1 = ptTmp - W * dirT.toPointF();

    glBegin(GL_LINE_STRIP);
    ViewPoint(pt0);
    ViewPoint(ptEnd);
    ViewPoint(pt1);
    glEnd();

    if (bBoth)
    {
        ptTmp = ptStart - L * dir.toPointF();
        pt0 = ptTmp + W * dirT.toPointF();
        pt1 = ptTmp - W * dirT.toPointF();

        glBegin(GL_LINE_STRIP);
        ViewPoint(pt0);
        ViewPoint(ptStart);
        ViewPoint(pt1);
        glEnd();
    }
}

void KWCtrlView::DrawSelectBox()
{
    if(!m_bShowSelBox)
        return;
    glColor3f(1.0f, 1.0f, 1.0f);
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(1, 0x0F0F);
    glBegin(GL_LINE_LOOP);
    glVertex2d(m_rectSelectingBox.left(), m_rectSelectingBox.top());
    glVertex2d(m_rectSelectingBox.right(), m_rectSelectingBox.top());
    glVertex2d(m_rectSelectingBox.right(), m_rectSelectingBox.bottom());
    glVertex2d(m_rectSelectingBox.left(), m_rectSelectingBox.bottom());
    glEnd();
    glDisable(GL_LINE_STIPPLE);
}

bool KWCtrlView::IsTextureVisiable(int i)
{
    return m_bTextureVisable[i];
}

void KWCtrlView::SetTextureVisiable(int i, bool isVisable)
{
    m_bTextureVisable[i] = isVisable;
}

void KWCtrlView::SetTextureOffset(int i, double x, double y)
{
    m_ptTextureOffset[i].rx() = x;
    m_ptTextureOffset[i].ry() = y;
}

void KWCtrlView::ViewPoint(const QPointF &_pt)
{
    glVertex2d(_pt.x(), _pt.y());
}

void KWCtrlView::ScrPint(const QPointF &pt)
{
    QPointF _pt = pt;
    Scr2View(_pt);
    glVertex2d(_pt.x(), _pt.y());
}

void KWCtrlView::initializeGL()
{
    QOpenGLWidget::initializeGL();
    initializeOpenGLFunctions();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //使用光滑着色时（即GL_SMOOTH），独立的处理图元中各个顶点的颜色
    glShadeModel(GL_SMOOTH);
    glClearColor(m_clrBackGround.red() / 255.0, m_clrBackGround.green() / 255.0, m_clrBackGround.blue() / 255.0, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearDepth(1.0f);

    //目标像素与当前像素在z方向上值大小比较
    glDepthFunc(GL_LEQUAL);

    //指定颜色和纹理坐标的差值质量  GL_NICEST高质量 GL_FASTEST高性能 GL_DONT_CARE不考虑
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    //抗锯齿
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    ResetView();

    Initialize();

    glGenTextures(TEXTURE_NUM, m_texture);
}

void KWCtrlView::resizeGL(int width, int height)
{
    m_painterPix = QPixmap(width, height);
    /*
    Opengl相关
    */
    GLsizei gWidth, gHeight;
    GLdouble aspect;
    gWidth = width;
    gHeight = height;

    if (height == 0)
    {
        aspect = (GLdouble)gWidth;
    }
    else
    {
        aspect = (GLdouble)gWidth / (GLdouble)gHeight;
    }
    int deltaX = m_bCoordMirrorX ? -1 : 1;
    int deltaY = m_bCoordMirrorY ? -1 : 1;
    glViewport(0, 0, gWidth, gHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-m_dViewH * aspect * deltaX, m_dViewH * aspect * deltaX, -m_dViewH * deltaY, m_dViewH * deltaY);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    update();
}

void KWCtrlView::paintGL()
{
    if(!isValid())
        return;
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    glPushMatrix();
    glScaled(m_fScale, m_fScale, m_fScale);
    glTranslated(m_dXTrans, m_dYTrans, 0.0);

    glLineWidth(m_fLineWidth);
    glPointSize(m_fPointSize);

    for(int i = 0; i < 2; ++i)
    {
        if(IsTextureVisiable(i))
        {
            glBindTexture(GL_TEXTURE_2D, m_texture[i]);
            DrawTexture(i);
        }
    }

    //!painter绘制到图片中
    m_painterPix.fill(QColor(Qt::transparent));
    QPainter painter(&m_painterPix);

    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(m_fontSong);
    painter.setPen(Qt::SolidLine);

    DrawOrigin(painter);
    DrawTick(painter);
    DrawCross();
    DrawGLSence(painter);
    DrawSelectBox();

    //!图片绘制结束
    painter.end();

    //////红蓝像素交换,镜像
    QImage image = m_painterPix.toImage().mirrored(false, true);
    glDrawPixels(image.width(), image.height(), GL_BGRA, GL_UNSIGNED_BYTE, image.bits());

    glPopMatrix();
    glFlush();
}

void KWCtrlView::mouseMoveEvent(QMouseEvent *event)
{
    QPointF ptScr = QPointF(event->pos().x(), event->pos().y());
    QPointF ptView = ptScr;
    Scr2View(ptView);
    if (m_bActiveMouse)
    {
        if(m_bLButtonDown)
        {//左键按下，移动鼠标
            m_ptEnd = ptView;
            m_rectSelectingBox.setCoords(m_ptStart.x(), m_ptStart.y(), m_ptEnd.x(), m_ptEnd.y());
        }
        else if(m_bRButtonDown)
        {//右键按下，移动鼠标
            m_ptEnd = ptView;
            m_ptEnd.rx() += m_dXTrans;
            m_ptEnd.ry() += m_dYTrans;

            m_dXTrans = m_dXTransOld + (m_ptEnd.x() - m_ptStart.x());
            m_dYTrans = m_dYTransOld + (m_ptEnd.y() - m_ptStart.y());
        }
        MouseMove(ptView, ptScr);

        m_ptCurCursor = ptView;
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void KWCtrlView::mousePressEvent(QMouseEvent *event)
{
    QPointF ptScr = QPointF(event->pos().x(), event->pos().y());
    QPointF ptView = ptScr;
    Scr2View(ptView);

    if(event->button() & Qt::LeftButton)
    {//左键按下
        m_ptStart  = m_ptEnd = ptView;
        m_bLButtonDown = true;
        MouseLButtonPressed(ptView, ptScr);
    }
    else if(event->button() & Qt::RightButton)
    {//鼠标右键
        //记录位置
        m_ptStart = ptView;
        m_ptStart.rx() += m_dXTrans;
        m_ptStart.ry() += m_dYTrans;
        m_ptEnd = m_ptStart;

        //记录之前坐标系偏移
        m_dXTransOld = m_dXTrans;
        m_dYTransOld = m_dYTrans;
        m_bRButtonDown = true;
        MouseRButtonPressed(ptView, ptScr);
    }
    update();
    QOpenGLWidget::mousePressEvent(event);
}

void KWCtrlView::mouseReleaseEvent(QMouseEvent *event)
{
    QPointF ptScr = QPointF(event->pos().x(), event->pos().y());
    QPointF ptView = ptScr;
    Scr2View(ptView);
    if(event->button() & Qt::LeftButton)
    {
        m_bLButtonDown = false;
        m_ptStart  = m_ptEnd = ptView;
        m_rectSelectingBox.setSize(QSize());//ptView, ptView);
        MouseLButtonReleased(ptView, ptScr);
    }
    else if(event->button() & Qt::RightButton)
    {
        m_ptEnd = ptView;
        m_ptEnd.rx() += m_dXTrans;
        m_ptEnd.ry() += m_dYTrans;
        m_bRButtonDown = false;
        MouseRButtonReleased(ptView, ptScr);
    }
    update();
    QOpenGLWidget::mouseReleaseEvent(event);
}

void KWCtrlView::wheelEvent(QWheelEvent *event)
{
    QPointF pt(event->pos().x(), event->pos().y());
    QPointF ptOrg, ptTar;

    QPointF ptView = pt;
    Scr2View(ptView);
    ptOrg.rx() = ptView.x();
    ptOrg.ry() = ptView.y();

    m_fScale += m_fScale * m_fZoomStep * event->delta() / 240;
    if (m_fScale < 0.0001f)
    {
        m_fScale = 0.0001f;
    }

    ptView = pt;
    Scr2View(ptView);
    ptTar.rx() = ptView.x();
    ptTar.ry() = ptView.y();

    m_dXTrans += (ptTar.x() - ptOrg.x());
    m_dYTrans += (ptTar.y() - ptOrg.y());

    update();
    QOpenGLWidget::wheelEvent(event);
}

void KWCtrlView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QPointF ptScr = QPointF(event->pos().x(), event->pos().y());
    QPointF ptView = ptScr;
    Scr2View(ptView);

    if(event->button() & Qt::LeftButton)
    {
        MouseLDoubleClick(ptView, ptScr);
    }
    else if(event->button() & Qt::RightButton)
    {
        MouseLDoubleClick(ptView, ptScr);
    }
}

void KWCtrlView::enterEvent(QEvent *evt)
{

    m_bActiveMouse = true;
    setMouseTracking(true);
    QOpenGLWidget::enterEvent(evt);
}

void KWCtrlView::leaveEvent(QEvent *evt)
{
    m_bActiveMouse = false;
    setMouseTracking(false);
    QOpenGLWidget::leaveEvent(evt);
    update();
}

void KWCtrlView::BindTexture(const QImage &img, int i)
{
    if(img.format() != QImage::Format_Indexed8)
    {
        //qDebug() << img.format();
        return;
    }
    m_imgTexture[i] = img;
    glBindTexture (GL_TEXTURE_2D, m_texture[i]);

    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, img.width(), img.height(), 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, img.bits());
}

void KWCtrlView::DrawTexture(int i)
{
    glEnable(GL_TEXTURE_2D);

//    glColor3f(0.8, 0.8, 0.8);

    glBegin(GL_POLYGON);

    glTexCoord2f(0.0, 0.0); glVertex2f(m_ptTextureOffset[i].x(), m_ptTextureOffset[i].y());
    glTexCoord2f(0.0, 1.0); glVertex2f(m_ptTextureOffset[i].x(), m_ptTextureOffset[i].y()+m_imgTexture[i].height());
    glTexCoord2f(1.0, 1.0); glVertex2f(m_ptTextureOffset[i].x()+m_imgTexture[i].width(), m_ptTextureOffset[i].y()+m_imgTexture[i].height());
    glTexCoord2f(1.0, 0.0); glVertex2f(m_ptTextureOffset[i].x()+m_imgTexture[i].width(), m_ptTextureOffset[i].y());

    glEnd();

    glDisable(GL_TEXTURE_2D);
}

void KWCtrlView::glColor(const QColor &clr)
{
    glColor3f(clr.red() / 255.0, clr.green() / 255.0, clr.blue() / 255.0);
}
# else

KWCtrlView::KWCtrlView(QWidget *parent)
    :QWidget(parent)
{
   m_tmUpdate.start(200, this);
}

KWCtrlView::~KWCtrlView()
{

}

void KWCtrlView::ResetView()
{
    m_matrix.reset();
}

void KWCtrlView::SetShapeSet(const GrShapeSetSPtr &pShapeSet)
{
    if(!pShapeSet->Size())
        return;
    Box box;
    pShapeSet->GetBBox(box);
    _window.setCoords(box.xmin(), box.ymin(), box.xmax(), box.ymax());
    //m_painter->setViewport(this->rect());
    //m_painter->scale(max(rect().width() / box.Width(), rect().height() / box.Height()), max(rect().width() / box.Width(), rect().height() / box.Height()));
    QPainterPath path;
    auto iterShape = pShapeSet->ShapeBegin();
    Shape* pShape = nullptr;
    std::vector<QPointF> pts;
    for (iterShape = pShapeSet->ShapeBegin(); iterShape != pShapeSet->ShapeEnd(); ++iterShape)
    {
        pts.clear();
        pShape = (*iterShape);
        pShape->GetPoints(pts);
        for (size_t i = 0; i < pts.size(); ++i )
        {
            i == 0 ? path.moveTo(pts[i].x() , pts[i].y()):
                     path.lineTo(pts[i].x() , pts[i].y());
        }
    }
    QPixmap pix(rect().width(),rect().height());
    pShapeSet->CreatePixmap(pix);
    m_pix.swap(pix);
    m_drawPath.swap(path);

}

void KWCtrlView::RestoreMode()
{

}

void KWCtrlView::UpdateMode()
{

}

void KWCtrlView::setSortedFlag(bool b)
{

}

void KWCtrlView::SetMultipSel(bool b)
{

}

void KWCtrlView::EnterDrawMode(int nMode)
{

}

void KWCtrlView::SetShapeTechnology(int nTech)
{

}

void KWCtrlView::paintEvent(QPaintEvent *evt)
{
    QPainter painter(this);
    QPen pen = QPen(QColor(Qt::green));
    painter.setBackground(QBrush(QColor(Qt::black)));
    pen.setWidth(50);
    painter.setPen(pen);
    painter.resetMatrix();
    painter.translate(offset, offset);

    painter.setViewport(rect());
    painter.setWindow(_window);
    painter.drawPixmap(_window, m_pix);
    painter.end();
    QWidget::paintEvent(evt);
}

void KWCtrlView::mouseMoveEvent(QMouseEvent *event)
{
    static int delta = 100;
    if(offset > 10000 || offset <= -1000)
        delta = -delta;

    offset += delta;
    //update();
    return QWidget::mouseMoveEvent(event);
}

void KWCtrlView::mousePressEvent(QMouseEvent *evt)
{
    if(evt->button() == Qt::LeftButton)
    {
        m_bLButtonDown = true;
    }
    else
    {
        m_bRButtonDown = true;
    }
}

void KWCtrlView::mouseReleaseEvent(QMouseEvent *evt)
{
    if(evt->button() == Qt::LeftButton)
    {
        m_bLButtonDown = false;
    }
    else
    {
        m_bRButtonDown = false;
    }
}

void KWCtrlView::timerEvent(QTimerEvent *evt)
{
    if(m_tmUpdate.timerId() == evt->timerId() && m_bLButtonDown)
    {
        update();
    }
    else
    {
        QWidget::timerEvent(evt);
    }
}




#endif
