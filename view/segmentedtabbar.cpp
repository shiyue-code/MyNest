#include "segmentedtabbar.h"

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

#include <algorithm>
#include <cmath>

SegmentedTabBar::SegmentedTabBar(QWidget* parent)
    : QWidget(parent)
    , slideAnimation(new QVariantAnimation(this))
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    slideAnimation->setDuration(260);
    slideAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(slideAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        selectedPosition = value.toReal();
        update();
    });
}

int SegmentedTabBar::addTab(const QString& text)
{
    tabTexts.push_back(text);
    if (selectedIndex < 0) {
        selectedIndex = 0;
        selectedPosition = 0.0;
    }
    updateGeometry();
    update();
    return tabTexts.size() - 1;
}

int SegmentedTabBar::currentIndex() const
{
    return selectedIndex;
}

void SegmentedTabBar::setCurrentIndex(int index)
{
    if (index < 0 || index >= tabTexts.size() || index == selectedIndex)
        return;

    selectedIndex = index;
    animateTo(index);
    emit currentChanged(index);
}

qreal SegmentedTabBar::indicatorPosition() const
{
    return selectedPosition;
}

void SegmentedTabBar::setIndicatorPosition(qreal position)
{
    selectedPosition = position;
    update();
}

QSize SegmentedTabBar::sizeHint() const
{
    return QSize(560, 78);
}

QSize SegmentedTabBar::minimumSizeHint() const
{
    return QSize(400, 72);
}

QRectF SegmentedTabBar::barRect() const
{
    const QFontMetrics metrics(font());
    int maxTabWidth = 164;
    for (const QString& text : tabTexts)
        maxTabWidth = std::max(maxTabWidth, metrics.horizontalAdvance(text) + 96);

    const int contentWidth = std::max(2, tabTexts.size()) * maxTabWidth + 10;
    const int widthHint = std::min(width() - 32, std::max(430, contentWidth));
    const int barWidth = std::max(1, widthHint);
    const int barHeight = 58;
    return QRectF((width() - barWidth) * 0.5, 10, barWidth, barHeight);
}

QRectF SegmentedTabBar::tabRect(int index) const
{
    if (index < 0 || index >= tabTexts.size())
        return {};

    const QRectF bar = barRect().adjusted(6, 6, -6, -6);
    const qreal tabWidth = bar.width() / std::max(1, tabTexts.size());
    return QRectF(bar.left() + tabWidth * index, bar.top(), tabWidth, bar.height());
}

int SegmentedTabBar::tabAt(const QPoint& pos) const
{
    for (int i = 0; i < tabTexts.size(); ++i) {
        if (tabRect(i).contains(pos))
            return i;
    }
    return -1;
}

void SegmentedTabBar::animateTo(int index)
{
    slideAnimation->stop();
    slideAnimation->setStartValue(selectedPosition);
    slideAnimation->setEndValue(static_cast<qreal>(index));
    slideAnimation->start();
}

void SegmentedTabBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bar = barRect();
    const qreal radius = bar.height() * 0.5;

    QPainterPath trackShadow;
    trackShadow.addRoundedRect(bar.adjusted(0, 2, 0, 5), radius, radius);
    painter.fillPath(trackShadow, QColor(15, 23, 42, 10));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#EAF0F7"));
    painter.drawRoundedRect(bar, radius, radius);

    if (!tabTexts.empty() && selectedIndex >= 0) {
        const int leftIndex = std::clamp(static_cast<int>(std::floor(selectedPosition)), 0, tabTexts.size() - 1);
        const int rightIndex = std::clamp(leftIndex + 1, 0, tabTexts.size() - 1);
        const qreal t = std::clamp(selectedPosition - leftIndex, 0.0, 1.0);
        const QRectF leftRect = tabRect(leftIndex);
        const QRectF rightRect = tabRect(rightIndex);
        const QRectF selectedRect(
            leftRect.left() + (rightRect.left() - leftRect.left()) * t,
            leftRect.top() + (rightRect.top() - leftRect.top()) * t,
            leftRect.width() + (rightRect.width() - leftRect.width()) * t,
            leftRect.height() + (rightRect.height() - leftRect.height()) * t);

        QPainterPath selectedShadow;
        selectedShadow.addRoundedRect(selectedRect.adjusted(0, 2, 0, 3), 21, 21);
        painter.fillPath(selectedShadow, QColor(37, 99, 235, 42));

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#2563EB"));
        painter.drawRoundedRect(selectedRect, selectedRect.height() * 0.5, selectedRect.height() * 0.5);
    }

    auto drawIcon = [&](const QRectF& iconRect, int index) {
        painter.save();
        painter.setPen(QPen(QColor(255, 255, 255, 150), 1.0));
        painter.setBrush(QColor(255, 255, 255, 44));
        painter.drawRoundedRect(iconRect, 6, 6);

        if (index == 0) {
            QPolygonF fixed;
            fixed << QPointF(iconRect.left() + 5, iconRect.bottom() - 8)
                  << QPointF(iconRect.left() + 9, iconRect.top() + 6)
                  << QPointF(iconRect.left() + 18, iconRect.top() + 7)
                  << QPointF(iconRect.left() + 15, iconRect.bottom() - 6);
            QPolygonF moving;
            moving << QPointF(iconRect.left() + 10, iconRect.bottom() - 5)
                   << QPointF(iconRect.right() - 5, iconRect.bottom() - 9)
                   << QPointF(iconRect.right() - 8, iconRect.top() + 11);

            painter.setBrush(QColor(255, 255, 255, 58));
            painter.setPen(QPen(QColor("#FFFFFF"), 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPolygon(fixed);
            painter.setPen(QPen(QColor("#BFDBFE"), 1.2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPolygon(moving);
            painter.setPen(QPen(QColor("#FCA5A5"), 1.5, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawArc(iconRect.adjusted(4, 4, -4, -4), 30 * 16, 220 * 16);
        } else {
            const QRectF board = iconRect.adjusted(4, 5, -4, -5);
            painter.setPen(QPen(QColor("#FFFFFF"), 1.2));
            painter.setBrush(QColor(255, 255, 255, 34));
            painter.drawRoundedRect(board, 3, 3);

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#C7D2FE"));
            painter.drawRoundedRect(QRectF(board.left() + 3, board.top() + 3, 7, 6), 2, 2);
            painter.setBrush(QColor("#A7F3D0"));
            painter.drawRoundedRect(QRectF(board.left() + 11, board.top() + 4, 6, 9), 2, 2);
            painter.setBrush(QColor("#FDE68A"));
            painter.drawRoundedRect(QRectF(board.left() + 4, board.top() + 11, 10, 5), 2, 2);
        }
        painter.restore();
    };

    for (int i = 0; i < tabTexts.size(); ++i) {
        const QRectF rect = tabRect(i);
        if (i == hoverIndex && i != selectedIndex) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(248, 250, 252, 180));
            painter.drawRoundedRect(rect, rect.height() * 0.5, rect.height() * 0.5);
        }

        QFont tabFont = font();
        tabFont.setPointSize(tabFont.pointSize() + 12);
        tabFont.setWeight(i == selectedIndex ? QFont::DemiBold : QFont::Medium);
        painter.setFont(tabFont);

        if (i == selectedIndex) {
            const QFontMetrics metrics(tabFont);
            const QSizeF iconSize(28, 28);
            const qreal contentWidth = iconSize.width() + 8 + metrics.horizontalAdvance(tabTexts[i]);
            const qreal startX = rect.center().x() - contentWidth * 0.5;
            const QRectF iconRect(startX, rect.center().y() - iconSize.height() * 0.5, iconSize.width(), iconSize.height());
            const QRectF textRect(iconRect.right() + 8, rect.top(), metrics.horizontalAdvance(tabTexts[i]), rect.height());
            drawIcon(iconRect, i);
            painter.setPen(QColor("#FFFFFF"));
            painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, tabTexts[i]);
        } else {
            painter.setPen(QColor("#64748B"));
            painter.drawText(rect, Qt::AlignCenter, tabTexts[i]);
        }
    }
}

void SegmentedTabBar::mouseMoveEvent(QMouseEvent* event)
{
    const int index = tabAt(event->pos());
    if (hoverIndex != index) {
        hoverIndex = index;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void SegmentedTabBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int index = tabAt(event->pos());
        if (index >= 0)
            setCurrentIndex(index);
    }
    QWidget::mousePressEvent(event);
}

void SegmentedTabBar::leaveEvent(QEvent* event)
{
    hoverIndex = -1;
    update();
    QWidget::leaveEvent(event);
}
