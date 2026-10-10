// Port of app/Tests/BoHLibrarianCoreTests/SkillQueryTests.swift (plan Task 5).
#include <QtTest/QtTest>

#include "SkillQuery.h"

#include <QStringList>

#include <algorithm>

using namespace boh;

namespace {
const QHash<qint64, QString> kPrincipleNames = {
    {5, QStringLiteral("Knock")}, {7, QStringLiteral("Moon")}, {8, QStringLiteral("Moth")},
    {10, QStringLiteral("Rose")}, {12, QStringLiteral("Sky")}, {1, QStringLiteral("Edge")},
};

Skill skill(const QString& name, qint64 id, std::optional<qint64> primary,
            std::optional<qint64> secondary, std::optional<int> level = {},
            bool language = false, std::optional<QString> wisdom = {},
            std::optional<QString> notes = {})
{
    Skill s;
    s.id = id;
    s.name = name;
    s.isLanguage = language;
    s.primaryPrincipleID = primary;
    s.secondaryPrincipleID = secondary;
    s.level = level;
    s.wisdom = wisdom;
    s.notes = notes;
    return s;
}

std::vector<Skill> roster()
{
    return {
        skill(QStringLiteral("Sky Stories"), 1, 12, 10, 3, false, QStringLiteral("Birdsong"),
              QStringLiteral("crafts Wind-in-Waiting")),
        skill(QStringLiteral("Vak"), 2, 5, 10, 2, true),
        skill(QStringLiteral("Edicts Martial"), 3, 7, 1),
        skill(QStringLiteral("Sacra Limiae"), 4, 8, 12, 5),
    };
}

SkillQueryOptions options(const QString& search = {}, std::optional<qint64> principle = {},
                          SkillKindFilter kind = SkillKindFilter::All,
                          SkillSort sort = SkillSort::Name)
{
    SkillQueryOptions o;
    o.searchText = search;
    o.principleID = principle;
    o.kindFilter = kind;
    o.sort = sort;
    return o;
}

QStringList sorted(QStringList list) { std::sort(list.begin(), list.end()); return list; }

QStringList apply(const SkillQueryOptions& o)
{
    QStringList names;
    for (const Skill& s : SkillFiltering::apply(roster(), o, kPrincipleNames))
        names << s.name;
    return names;
}
} // namespace

class SkillQueryTests final : public QObject {
    Q_OBJECT

private slots:
    // MARK: Search

    void searchMatchesNameWisdomAndNotes()
    {
        QCOMPARE(apply(options(QStringLiteral("stories"))),
                 (QStringList{QStringLiteral("Sky Stories")}));
        QCOMPARE(apply(options(QStringLiteral("birdsong"))),
                 (QStringList{QStringLiteral("Sky Stories")}));
        QCOMPARE(apply(options(QStringLiteral("wind-in-waiting"))),
                 (QStringList{QStringLiteral("Sky Stories")}));
        QCOMPARE(apply(options(QStringLiteral("vak"))), (QStringList{QStringLiteral("Vak")}));
    }

    // MARK: Filters

    void principleFilterMatchesPrimaryOrSecondary()
    {
        // Sky appears as Sky Stories' primary AND Sacra Limiae's secondary.
        QCOMPARE(sorted(apply(options({}, 12))),
                 (QStringList{QStringLiteral("Sacra Limiae"), QStringLiteral("Sky Stories")}));
        QCOMPARE(sorted(apply(options({}, 10))),
                 (QStringList{QStringLiteral("Sky Stories"), QStringLiteral("Vak")}));
    }

    void kindFilterSeparatesLanguagesFromSkills()
    {
        QCOMPARE(apply(options({}, {}, SkillKindFilter::Languages)),
                 (QStringList{QStringLiteral("Vak")}));
        QCOMPARE(apply(options({}, {}, SkillKindFilter::Skills)).size(), 3);
        QCOMPARE(apply(options({}, {}, SkillKindFilter::All)).size(), 4);
    }

    // MARK: Sorts

    void sortByLevelDescendingWithUnknownsLast()
    {
        QCOMPARE(apply(options({}, {}, SkillKindFilter::All, SkillSort::Level)),
                 (QStringList{QStringLiteral("Sacra Limiae"), QStringLiteral("Sky Stories"),
                              QStringLiteral("Vak"), QStringLiteral("Edicts Martial")}));
    }

    void sortByNameAndRecent()
    {
        QCOMPARE(apply(options({}, {}, SkillKindFilter::All, SkillSort::Name)).first(),
                 QStringLiteral("Edicts Martial"));
        QCOMPARE(apply(options({}, {}, SkillKindFilter::All, SkillSort::Recent)).first(),
                 QStringLiteral("Sacra Limiae"));
    }

    // MARK: Skill math

    void levelOneSkillShowsTwoPrimaryOneSecondary()
    {
        const auto contributions = SkillMath::contributions(1);
        QCOMPARE(contributions.primary, 2);
        QCOMPARE(contributions.secondary, 1);
    }

    void levelNineSkillShowsTenAndNine()
    {
        const auto contributions = SkillMath::contributions(9);
        QCOMPARE(contributions.primary, 10);
        QCOMPARE(contributions.secondary, 9);
    }
};

QTEST_MAIN(SkillQueryTests)
#include "test_SkillQuery.moc"
