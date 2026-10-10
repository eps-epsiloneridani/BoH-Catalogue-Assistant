// Port of ColorMath.swift — WCAG 2.1 color math backing the UI's contrast
// rules (docs/DECISIONS.md D12). Pure math on purpose: the app derives badge
// display colors from the user-editable principle palette per appearance mode.
#pragma once

#include <QString>
#include <QtGlobal>

#include <optional>

namespace boh {

class ColorMath {
public:
    struct Channels {
        int r;
        int g;
        int b;
        bool operator==(const Channels&) const = default;
    };

    /// Approximate desktop window background per appearance — the capsule/badge
    /// backdrop these colors must read against.
    static QString lightWindowBackground() { return QStringLiteral("#ECECEC"); }
    static QString darkWindowBackground() { return QStringLiteral("#282828"); }

    static std::optional<Channels> channels(const QString& hex);
    static QString hex(Channels channels);

    /// WCAG 2.1 relative luminance of an sRGB "#RRGGBB" (nullopt when malformed).
    static std::optional<double> relativeLuminance(const QString& hex);

    /// WCAG contrast ratio (1…21); nullopt when either hex is malformed.
    static std::optional<double> contrastRatio(const QString& a, const QString& b);

    /// Alpha-blend "#RRGGBB" foreground over "#RRGGBB" background; "#RRGGBB" out.
    static std::optional<QString> blend(const QString& foreground, const QString& background,
                                        double alpha);

    /// The text color for tint-identified UI (badges: fill/border keep the tint as
    /// the visual identity; the *text* must read over the 18%-tint capsule this
    /// UI paints). Returns `tint` unchanged when it already meets `target`;
    /// otherwise the nearest mix toward black (light mode) or white (dark mode)
    /// that does. Nullopt when `tint` is malformed (callers fall back to palette text).
    static std::optional<QString> readableTextHex(const QString& tint, bool darkMode,
                                                  double target = 4.5);
};

} // namespace boh
