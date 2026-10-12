// Remediation R1 (D12): captions/section titles previously used palette(mid) —
// that role is a disabled-text hint and reads unreadable on several themes
// (user bug report 1). This helper derives the dimmest mix of WindowText toward
// Window that still passes WCAG AA (4.5:1) — the same derive-don't-bake posture
// as ColorMath on the badge path.
#pragma once

#include <QColor>
#include <QString>
class QWidget;
#include <QPalette>

namespace boh {

class ThemedText {
public:
    /// WCAG relative luminance of a QColor (0…1) — thin shim over ColorMath's math.
    static double relativeLuminance(const QColor& color);

    /// The quiet-but-readable caption color for this palette.
    static QColor secondaryTextColor(const QPalette& palette);

    /// Convenience for setStyleSheet call sites: the derived color as "#RRGGBB".
    static QString secondaryColorHex(const QWidget* widget);
};

} // namespace boh