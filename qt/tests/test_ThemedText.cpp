// D12 for Qt: secondary/caption text must ALSO read at 4.5:1 — the port reused
// `palette(mid)` (a disabled-text hint), which is unreadable on several themes
// (user bug report 1, remediation R1). The helper derives the dimmest mix of
// WindowText toward Window that still passes AA — enforced here across palettes.
#include <QtTest/QtTest>

#include "Shared/ThemedText.h"

#include <QColor>
#include <QPalette>

using boh::ThemedText;

namespace {
bool passesAA(const QColor& text, const QColor& background)
{
    const double lr = ThemedText::relativeLuminance(text);
    const double lb = ThemedText::relativeLuminance(background);
    const double hi = std::max(lr, lb);
    const double lo = std::min(lr, lb);
    return (hi + 0.05) / (lo + 0.05) >= 4.5;
}

QPalette paletteWith(const QColor& window, const QColor& text)
{
    QPalette p;
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    return p;
}
} // namespace

class ThemedTextTests final : public QObject {
    Q_OBJECT

private slots:
    /// The enforcement, mirroring ColorMathTests: for every palette we can name,
    /// the derived secondary text reads >= 4.5:1 over Window.
    void secondaryTextMeetsAAOnEveryPalette()
    {
        const std::vector<std::pair<QPalette, const char*>> palettes = {
            {paletteWith(QColor(0xEC, 0xEC, 0xEC), QColor(0, 0, 0)), "light"},
            {paletteWith(QColor(0x28, 0x28, 0x28), QColor(0xFF, 0xFF, 0xFF)), "dark"},
            {paletteWith(QColor(0x1E, 0x1E, 0x1E), QColor(0xC0, 0xC0, 0xC0)), "dark/highpoint"},
            {paletteWith(QColor(0xFF, 0xFF, 0xFF), QColor(0x60, 0x60, 0x60)), "light/low-medium"},
            {paletteWith(QColor(0x35, 0x40, 0x2B), QColor(0xE0, 0xE0, 0xE0)), "tinted dark"},
            {paletteWith(QColor(0xF5, 0xF0, 0xE8), QColor(0x40, 0x38, 0x2A)), "tinted light"},
        };
        for (const auto& [palette, name] : palettes) {
            const QColor secondary = ThemedText::secondaryTextColor(palette);
            const QColor window = palette.color(QPalette::Window);
            QVERIFY2(passesAA(secondary, window),
                     qPrintable(QStringLiteral("%1: secondary %2 reads %3 over %4")
                                    .arg(name)
                                    .arg(secondary.name())
                                    .arg(secondary.name())
                                    .arg(window.name())));
        }
    }

    /// "Dimmer than the primary text" is the point — the helper must be a quiet
    /// secondary, not a second primary. It must be strictly closer to the window
    /// background than WindowText is.
    void secondaryTextIsDimmerThanPrimary()
    {
        for (const auto& palette :
             {paletteWith(QColor(0xEC, 0xEC, 0xEC), QColor(0, 0, 0)),
              paletteWith(QColor(0x28, 0x28, 0x28), QColor(0xFF, 0xFF, 0xFF))}) {
            const QColor secondary = ThemedText::secondaryTextColor(palette);
            const double windowLightness = palette.color(QPalette::Window).lightnessF();
            const double textLightness = palette.color(QPalette::WindowText).lightnessF();
            const double secondaryLightness = secondary.lightnessF();
            QVERIFY2(std::abs(secondaryLightness - windowLightness)
                         < std::abs(textLightness - windowLightness),
                     "secondary sits between primary text and the window background");
        }
    }
};

QTEST_MAIN(ThemedTextTests)
#include "test_ThemedText.moc"