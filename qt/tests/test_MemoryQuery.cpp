// Port of app/Tests/BoHLibrarianCoreTests/MemoryQueryTests.swift (plan Task 5).
#include <QtTest/QtTest>

#include "MemoryQuery.h"

#include <QStringList>

#include <algorithm>

using namespace boh;

namespace {
const QHash<qint64, QString> kPrincipleNames = {
    {2, QStringLiteral("Heart")},  {5, QStringLiteral("Knock")},   {6, QStringLiteral("Lantern")},
    {7, QStringLiteral("Moon")},   {8, QStringLiteral("Moth")},    {10, QStringLiteral("Rose")},
    {12, QStringLiteral("Sky")},   {13, QStringLiteral("Winter")},
};

Aspect aspect(qint64 principle, int level)
{
    return Aspect{principle, kPrincipleNames.value(principle, QStringLiteral("?")), level};
}

std::vector<Memory> cards()
{
    Memory horizon;
    horizon.id = 1;
    horizon.name = QStringLiteral("Horizon-Sight");
    horizon.kind = MemoryKind::Memory;
    horizon.persistent = true;
    horizon.aspects = {aspect(10, 4)};

    Memory fog;
    fog.id = 2;
    fog.name = QStringLiteral("Fog");
    fog.kind = MemoryKind::Weather;
    fog.aspects = {aspect(5, 3), aspect(7, 3)};

    Memory numen;
    numen.id = 3;
    numen.name = QStringLiteral("Numen: That Old Lost Music");
    numen.kind = MemoryKind::Numen;
    numen.persistent = true;
    numen.aspects = {aspect(10, 5), aspect(12, 5), aspect(13, 5)};

    Memory hunch;
    hunch.id = 4;
    hunch.name = QStringLiteral("Curious Hunch");
    hunch.persistent = true;
    hunch.notes = QStringLiteral("from swimming at Sea's Edge");
    hunch.aspects = {aspect(2, 3), aspect(5, 4), aspect(6, 3), aspect(8, 3)};

    return {horizon, fog, numen, hunch};
}

MemoryQueryOptions options(const QString& search = {}, std::optional<qint64> principle = {},
                           std::optional<int> minLevel = {}, MemorySort sort = MemorySort::Name)
{
    MemoryQueryOptions o;
    o.searchText = search;
    o.principleID = principle;
    o.minLevel = minLevel;
    o.sort = sort;
    return o;
}

QStringList sorted(QStringList list) { std::sort(list.begin(), list.end()); return list; }

QStringList apply(const MemoryQueryOptions& o)
{
    QStringList names;
    for (const Memory& m : MemoryFiltering::apply(cards(), o, kPrincipleNames))
        names << m.name;
    return names;
}
} // namespace

class MemoryQueryTests final : public QObject {
    Q_OBJECT

private slots:
    // MARK: Search

    void searchMatchesNameNotesKindAndAspectText()
    {
        QCOMPARE(apply(options(QStringLiteral("hunch"))),
                 (QStringList{QStringLiteral("Curious Hunch")}));
        QCOMPARE(apply(options(QStringLiteral("swimming"))),
                 (QStringList{QStringLiteral("Curious Hunch")}));
        QCOMPARE(apply(options(QStringLiteral("weather"))), (QStringList{QStringLiteral("Fog")}));
        QCOMPARE(sorted(apply(options(QStringLiteral("rose")))),
                 (QStringList{QStringLiteral("Horizon-Sight"), QStringLiteral("Numen: That Old Lost Music")}));
        QCOMPARE(apply(options(QStringLiteral("knock 4"))),
                 (QStringList{QStringLiteral("Curious Hunch")}));
    }

    void emptySearchReturnsEverythingAndSearchIsTrimmed()
    {
        QCOMPARE(apply(options()).size(), 4);
        QCOMPARE(apply(options(QStringLiteral("   "))).size(), 4);
        QCOMPARE(apply(options(QStringLiteral("zzz"))).size(), 0);
    }

    // MARK: Filters

    void principleFilter()
    {
        QCOMPARE(sorted(apply(options({}, 10))),
                 (QStringList{QStringLiteral("Horizon-Sight"), QStringLiteral("Numen: That Old Lost Music")}));
        QCOMPARE(apply(options({}, 7)), (QStringList{QStringLiteral("Fog")}));
        QCOMPARE(apply(options({}, 13)), (QStringList{QStringLiteral("Numen: That Old Lost Music")}));
    }

    void principleFilterWithMinLevel()
    {
        QCOMPARE(apply(options({}, 10, 5)),
                 (QStringList{QStringLiteral("Numen: That Old Lost Music")}));
        QCOMPARE(apply(options({}, 5, 4)), (QStringList{QStringLiteral("Curious Hunch")}));
    }

    void minLevelWithoutPrincipleUsesHighestAspect()
    {
        QCOMPARE(apply(options({}, {}, 5)),
                 (QStringList{QStringLiteral("Numen: That Old Lost Music")}));
        QCOMPARE(sorted(apply(options({}, {}, 4))),
                 (QStringList{QStringLiteral("Curious Hunch"), QStringLiteral("Horizon-Sight"),
                              QStringLiteral("Numen: That Old Lost Music")}));
        QCOMPARE(apply(options({}, {}, 6)).size(), 0);
    }

    // MARK: Sorts

    void sortByNameIsLocalized()
    {
        QCOMPARE(apply(options({}, {}, {}, MemorySort::Name)).first(),
                 QStringLiteral("Curious Hunch"));
    }

    void sortByLevelUsesFilteredPrincipleWhenSet()
    {
        // Filtering on Knock: Curious Hunch (Knock 4) before Fog (Knock 3).
        QCOMPARE(apply(options({}, 5, {}, MemorySort::Level)),
                 (QStringList{QStringLiteral("Curious Hunch"), QStringLiteral("Fog")}));
    }

    void sortByLevelWithoutPrincipleUsesHighestAspect()
    {
        QCOMPARE(apply(options({}, {}, {}, MemorySort::Level)),
                 (QStringList{QStringLiteral("Numen: That Old Lost Music"), QStringLiteral("Curious Hunch"),
                              QStringLiteral("Horizon-Sight"), QStringLiteral("Fog")}));
    }

    void sortByKindPutsNuminaFirstThenWeatherThenMemories()
    {
        const QStringList names = apply(options({}, {}, {}, MemorySort::Kind));
        QCOMPARE(names.first(), QStringLiteral("Numen: That Old Lost Music"));
        QCOMPARE(names[1], QStringLiteral("Fog")); // weather after numina
        QCOMPARE(names[2], QStringLiteral("Curious Hunch")); // then memories alphabetically
        QCOMPARE(names.last(), QStringLiteral("Horizon-Sight"));
    }

    void sortByRecency()
    {
        QCOMPARE(apply(options({}, {}, {}, MemorySort::Recent)).first(),
                 QStringLiteral("Curious Hunch"));
    }
};

QTEST_MAIN(MemoryQueryTests)
#include "test_MemoryQuery.moc"
