#include "ThemedText.h"

#include "ColorMath.h"

#include <QWidget>

#include <algorithm>
#include <cmath>

namespace boh {

double ThemedText::relativeLuminance(const QColor& color)
{
    const auto linear = [](double v) {
        v /= 255.0;
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.red() * 1.0) + 0.7152 * linear(color.green() * 1.0)
           + 0.0722 * linear(color.blue() * 1.0);
}

QColor ThemedText::secondaryTextColor(const QPalette& palette)
{
    const QColor window = palette.color(QPalette::Window);
    const QColor text = palette.color(QPalette::WindowText);

    const auto contrast = [&](const QColor& a, const QColor& b) {
        const double la = relativeLuminance(a);
        const double lb = relativeLuminance(b);
        const double hi = std::max(la, lb);
        const double lo = std::min(la, lb);
        return (hi + 0.05) / (lo + 0.05);
    };

    // All channels in 0…255, one scale. Walk from the primary text toward the
    // window background, dimming until AA would fail; keep the last passing
    // step (the quietest readable color).
    const auto mix = [&](double step) {
        return QColor(int(std::round(text.red() + (window.red() - text.red()) * step)),
                      int(std::round(text.green() + (window.green() - text.green()) * step)),
                      int(std::round(text.blue() + (window.blue() - text.blue()) * step)));
    };

    QColor best = text;
    for (double step = 0.10; step <= 1.0 + 1e-9; step += 0.05) {
        const QColor candidate = mix(step);
        if (contrast(candidate, window) >= 4.5)
            best = candidate;
        else
            break;
    }
    return best;
}

QString ThemedText::secondaryColorHex(const QWidget* widget)
{
    QPalette palette = widget ? widget->palette() : QPalette();
    return secondaryTextColor(palette).name();
}

} // namespace boh