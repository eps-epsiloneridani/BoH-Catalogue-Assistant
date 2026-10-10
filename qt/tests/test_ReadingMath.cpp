// Port of app/Tests/BoHLibrarianCoreTests/ReadingMathTests.swift (plan Task 4).
// The Reading Helper's user-facing sentences, pinned by tests.
#include <QtTest/QtTest>

#include "ReadingMath.h"

using boh::ReadingMath;

class ReadingMathTests final : public QObject {
    Q_OBJECT

private slots:
    // MARK: Requirement line

    void requirementLineComplete()
    {
        QCOMPARE(ReadingMath::requirementLine(QStringLiteral("Rose"), 6),
                 QStringLiteral("You need Rose 6."));
    }

    void requirementLineMissingPrinciple()
    {
        QCOMPARE(ReadingMath::requirementLine(std::nullopt, 10),
                 QStringLiteral("Difficulty 10 recorded, but not its principle — "
                                "note the mystery principle on the Books screen to compute candidates."));
    }

    void requirementLineMissingDifficulty()
    {
        QCOMPARE(ReadingMath::requirementLine(QStringLiteral("Rose"), std::nullopt),
                 QStringLiteral("The mystery is Rose, but its difficulty isn't recorded yet."));
    }

    void requirementLineNothingRecorded()
    {
        QCOMPARE(ReadingMath::requirementLine(std::nullopt, std::nullopt),
                 QStringLiteral("Not catalogued yet — record its mystery on the Books screen."));
    }

    // MARK: Reach line

    void reachLineEnoughWithMemoryAndSkill()
    {
        QCOMPARE(*ReadingMath::reachLine(6, 4, 3),
                 QStringLiteral("Best recorded: memory 4 + skill 3 = 7 — enough, before souls, inks and tools."));
    }

    void reachLineExactlyEnough()
    {
        QCOMPARE(*ReadingMath::reachLine(6, 6, std::nullopt),
                 QStringLiteral("Best recorded: memory 6 = 6 — enough, before souls, inks and tools."));
    }

    void reachLineShortWithSkillOnly()
    {
        QCOMPARE(*ReadingMath::reachLine(10, std::nullopt, 4),
                 QStringLiteral("Best recorded: skill 4 = 4 — 6 short, before souls, inks and tools."));
    }

    void reachLineNothingRecorded()
    {
        QCOMPARE(*ReadingMath::reachLine(4, std::nullopt, std::nullopt),
                 QStringLiteral("Nothing recorded yet reaches for it."));
    }

    void reachLineNeedsADifficulty()
    {
        QVERIFY(!ReadingMath::reachLine(std::nullopt, 4, 3).has_value());
    }
};

QTEST_MAIN(ReadingMathTests)
#include "test_ReadingMath.moc"
