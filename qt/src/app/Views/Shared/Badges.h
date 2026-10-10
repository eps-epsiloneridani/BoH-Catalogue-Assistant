// Shared badge painting (D12): tinted capsule with AA-derived text color from
// ColorMath — fill/border keep the tint identity, the text is derived per mode.
#pragma once

#include <QPainter>
#include <QString>

class QWidget;

namespace boh {

class Badges {
public:
    /// True when the widget's palette suggests dark mode (drives AA text).
    static bool isDarkMode(const QWidget* widget);

    static QSize badgeSize(const QString& text, const QFontMetrics& metrics);
    static void paint(QPainter* painter, const QRect& rect, const QString& text,
                      const QString& tintHex, bool darkMode);
};

} // namespace boh
