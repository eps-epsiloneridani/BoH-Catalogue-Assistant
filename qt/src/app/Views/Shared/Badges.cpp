#include "Badges.h"

#include "ColorMath.h"

#include <QPalette>
#include <QWidget>

namespace boh {

bool Badges::isDarkMode(const QWidget* widget)
{
    return widget->palette().color(QPalette::Window).lightness() < 128;
}

QSize Badges::badgeSize(const QString& text, const QFontMetrics& metrics)
{
    const int width = metrics.horizontalAdvance(text) + 16;
    return {width, metrics.height() + 6};
}

void Badges::paint(QPainter* painter, const QRect& rect, const QString& text,
                   const QString& tintHex, bool darkMode)
{
    const auto windowBackground = darkMode ? ColorMath::darkWindowBackground()
                                           : ColorMath::lightWindowBackground();
    const QString backdrop = ColorMath::blend(tintHex, windowBackground, 0.18)
                                 .value_or(windowBackground);
    const QString textColor =
        ColorMath::readableTextHex(tintHex, darkMode).value_or(darkMode ? QStringLiteral("#FFFFFF")
                                                                        : QStringLiteral("#000000"));

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(backdrop));
    painter->drawRoundedRect(rect, 6, 6);
    painter->setPen(QColor(textColor));
    painter->drawText(rect, Qt::AlignCenter, text);
    painter->restore();
}

} // namespace boh
