#ifndef SEGMENTEDTABBAR_H
#define SEGMENTEDTABBAR_H

#include <QWidget>

#include <QStringList>

class QVariantAnimation;

class SegmentedTabBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal indicatorPosition READ indicatorPosition WRITE setIndicatorPosition)

public:
    explicit SegmentedTabBar(QWidget* parent = nullptr);

    int addTab(const QString& text);
    int currentIndex() const;
    void setCurrentIndex(int index);

    qreal indicatorPosition() const;
    void setIndicatorPosition(qreal position);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void currentChanged(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRectF barRect() const;
    QRectF tabRect(int index) const;
    int tabAt(const QPoint& pos) const;
    void animateTo(int index);

    QStringList tabTexts;
    QVariantAnimation* slideAnimation = nullptr;
    int selectedIndex = -1;
    int hoverIndex = -1;
    qreal selectedPosition = 0.0;
};

#endif // SEGMENTEDTABBAR_H
