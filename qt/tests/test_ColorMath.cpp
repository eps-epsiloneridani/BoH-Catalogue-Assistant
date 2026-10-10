// Port of app/Tests/BoHLibrarianCoreTests/ColorMathTests.swift (plan Task 4).
// WCAG math backing badge contrast (D12): every seeded principle must read at
// 4.5:1 over its own capsule backdrop in BOTH appearance modes.
#include <QtTest/QtTest>

#include "ColorMath.h"

#include <cmath>
#include <vector>

using boh::ColorMath;

namespace {
const std::vector<const char*> kSeededTints = {
    "#7B241C", "#D35400", "#884EA0", "#E74C3C", "#148F77", "#F4D03F",
    "#5D6D7E", "#717D7E", "#27AE60", "#EC87C0", "#6E2C00", "#5DADE2", "#85C1E9",
};
bool close(double a, double b, double eps) { return std::abs(a - b) < eps; }
} // namespace

class ColorMathTests final : public QObject {
    Q_OBJECT

private slots:
    void channelParsingAndMalformedInput()
    {
        const auto prefixed = ColorMath::channels(QStringLiteral("#1A2B3C"));
        QVERIFY(prefixed.has_value());
        const bool prefixedEq = *prefixed == ColorMath::Channels{0x1A, 0x2B, 0x3C};
        QVERIFY(prefixedEq);
        const auto bare = ColorMath::channels(QStringLiteral("1A2B3C"));
        QVERIFY(bare.has_value());
        const bool bareEq = *bare == ColorMath::Channels{0x1A, 0x2B, 0x3C};
        QVERIFY2(bareEq, "# optional");
        QVERIFY(!ColorMath::channels(QStringLiteral("nope")).has_value());
        QVERIFY(!ColorMath::channels(QStringLiteral("#12345")).has_value());
        QVERIFY(!ColorMath::channels(QStringLiteral("#1A2B3G")).has_value());
        QVERIFY(!ColorMath::relativeLuminance(QStringLiteral("nope")).has_value());
        QVERIFY(!ColorMath::contrastRatio(QStringLiteral("#000000"), QStringLiteral("nope")).has_value());
        QVERIFY(!ColorMath::blend(QStringLiteral("#000000"), QStringLiteral("nope"), 0.5).has_value());
        QVERIFY(!ColorMath::readableTextHex(QStringLiteral("nope"), false).has_value());
    }

    void luminanceAndContrastReferenceValues()
    {
        QVERIFY(close(*ColorMath::relativeLuminance(QStringLiteral("#000000")), 0, 0.0001));
        QVERIFY(close(*ColorMath::relativeLuminance(QStringLiteral("#FFFFFF")), 1, 0.0001));
        QVERIFY(close(*ColorMath::contrastRatio(QStringLiteral("#000000"), QStringLiteral("#FFFFFF")), 21, 0.01));
        // #767676 is the canonical 4.5:1 grey against white.
        QVERIFY(close(*ColorMath::contrastRatio(QStringLiteral("#767676"), QStringLiteral("#FFFFFF")), 4.54, 0.05));
    }

    void blending()
    {
        QCOMPARE(*ColorMath::blend(QStringLiteral("#FFFFFF"), QStringLiteral("#000000"), 0), QStringLiteral("#000000"));
        QCOMPARE(*ColorMath::blend(QStringLiteral("#FFFFFF"), QStringLiteral("#000000"), 1), QStringLiteral("#FFFFFF"));
        // 0x7B (123) at 18% over black: 22.14 -> 22 = 0x16.
        QCOMPARE(*ColorMath::blend(QStringLiteral("#7B0000"), QStringLiteral("#000000"), 0.18),
                 QStringLiteral("#160000"));
        // 123*0.18 + 255*0.82 = 231.24 -> 231 = 0xE7.
        QCOMPARE(*ColorMath::blend(QStringLiteral("#7B0000"), QStringLiteral("#FF0000"), 0.18),
                 QStringLiteral("#E70000"));
    }

    /// The enforcement: every seeded principle reads at 4.5:1 over its own capsule
    /// backdrop in BOTH appearance modes (the raw tints fail 13/13 in some mode).
    void readableTextMeetsAAForEverySeededPrincipleInBothModes()
    {
        for (const char* rawTint : kSeededTints) {
            const QString tint = QString::fromUtf8(rawTint);
            for (const bool darkMode : {false, true}) {
                const QString text = ColorMath::readableTextHex(tint, darkMode)
                                         .value_or(QString(QStringLiteral("<nullopt> ")));
                const QString windowBackground = darkMode ? ColorMath::darkWindowBackground()
                                                          : ColorMath::lightWindowBackground();
                const QString backdrop = ColorMath::blend(tint, windowBackground, 0.18)
                                             .value_or(QString());
                const double ratio = ColorMath::contrastRatio(text, backdrop).value_or(0.0);
                QVERIFY2(ratio >= 4.5,
                         qPrintable(QStringLiteral("%1 (darkMode=%2) text %3 reads %4")
                                        .arg(tint).arg(darkMode).arg(text).arg(ratio)));
            }
        }
    }

    void readableTextKeepsAlreadyReadableTintsUntouched()
    {
        // Edge (#7B241C) and Scale (#6E2C00) pass light mode as-is; not dark.
        QCOMPARE(*ColorMath::readableTextHex(QStringLiteral("#7B241C"), false), QStringLiteral("#7B241C"));
        QCOMPARE(*ColorMath::readableTextHex(QStringLiteral("#6E2C00"), false), QStringLiteral("#6E2C00"));
        // Lantern (#F4D03F) reads well on dark and is untouched there.
        QCOMPARE(*ColorMath::readableTextHex(QStringLiteral("#F4D03F"), true), QStringLiteral("#F4D03F"));
        // …and is lifted in light mode (it read 1.21:1 as raw tint text).
        QVERIFY(*ColorMath::readableTextHex(QStringLiteral("#F4D03F"), false) != QStringLiteral("#F4D03F"));
        // Extreme stop: black tint under light mode stays black (already max).
        QCOMPARE(*ColorMath::readableTextHex(QStringLiteral("#000000"), false), QStringLiteral("#000000"));
    }
};

QTEST_MAIN(ColorMathTests)
#include "test_ColorMath.moc"
