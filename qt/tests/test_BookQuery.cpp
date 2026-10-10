// Port of app/Tests/BoHLibrarianCoreTests/BookQueryTests.swift (plan Task 5).
// Pure-logic tests for the Books screen: filtering, sorting, search, form
// mastery transitions, and the journal text composed by the record-a-read flow.
#include <QtTest/QtTest>

#include "BookQuery.h"

#include <QStringList>

using namespace boh;

namespace {
const QHash<qint64, QString> kPrincipleNames = {{10, QStringLiteral("Rose")},
                                                {11, QStringLiteral("Scale")},
                                                {12, QStringLiteral("Sky")}};
const QHash<qint64, QString> kLanguageNames = {{3, QStringLiteral("Latin")},
                                               {7, QStringLiteral("Fucine")}};

Book book(const QString& title, qint64 id, std::optional<qint64> mysteryPrinciple = {},
          std::optional<int> difficulty = {}, std::optional<qint64> language = {},
          ReadStatus status = ReadStatus::Uncatalogued,
          std::optional<Contamination> contamination = {},
          std::optional<QString> set = {}, std::optional<QString> notes = {})
{
    Book b;
    b.id = id;
    b.title = title;
    b.setName = set;
    b.bookKind = BookKind::Book;
    b.languageID = language;
    b.mysteryPrincipleID = mysteryPrinciple;
    b.difficulty = difficulty;
    b.readStatus = status;
    b.contamination = contamination;
    b.notes = notes;
    return b;
}

std::vector<Book> library()
{
    return {
        book(QStringLiteral("The Turquoise Hand"), 1, 10, 10, 7, ReadStatus::Catalogued, {},
             QStringLiteral("Numen books"), QStringLiteral("persistent Rose memory inside")),
        book(QStringLiteral("Annals of St Brandans"), 2, 11, 4, 3, ReadStatus::Mastered,
             Contamination::None),
        book(QStringLiteral("An Introduction to Histories"), 3, {}, {}, 3),
        book(QStringLiteral("De Bellis Murorum"), 4, 12, 6, {}, ReadStatus::Uncatalogued,
             Contamination::Curse),
    };
}

BookQueryOptions options(const QString& search = {}, BookStatusFilter filter = BookStatusFilter::All,
                         BookSort sort = BookSort::Title,
                         std::optional<qint64> mysteryPrincipleID = {})
{
    BookQueryOptions o;
    o.searchText = search;
    o.statusFilter = filter;
    o.mysteryPrincipleID = mysteryPrincipleID;
    o.sort = sort;
    return o;
}

QStringList apply(const BookQueryOptions& o)
{
    QStringList titles;
    for (const Book& b : BookFiltering::apply(library(), o, kPrincipleNames, kLanguageNames))
        titles << b.title;
    return titles;
}
} // namespace

class BookQueryTests final : public QObject {
    Q_OBJECT

private slots:
    // MARK: Filters

    /// Mystery filter (Reading Helper): only books whose mystery is that
    /// principle; books with no recorded principle drop out while filtered.
    void mysteryPrincipleFilter()
    {
        QCOMPARE(apply(options({}, BookStatusFilter::All, BookSort::Title, 10)),
                 (QStringList{QStringLiteral("The Turquoise Hand")}));
        QCOMPARE(apply(options({}, BookStatusFilter::All, BookSort::Title, 11)),
                 (QStringList{QStringLiteral("Annals of St Brandans")}));
    }

    /// Easiest-first: ascending difficulty, unknown last; title breaks ties.
    void easiestFirstSort()
    {
        auto extra = library();
        extra.push_back(book(QStringLiteral("Cheap Read"), 5, 12, 2));
        BookQueryOptions o;
        o.sort = BookSort::Easiest;
        QStringList titles;
        for (const Book& b : BookFiltering::apply(extra, o, kPrincipleNames, kLanguageNames))
            titles << b.title;
        QCOMPARE(titles, (QStringList{QStringLiteral("Cheap Read"), QStringLiteral("Annals of St Brandans"),
                                      QStringLiteral("De Bellis Murorum"), QStringLiteral("The Turquoise Hand"),
                                      QStringLiteral("An Introduction to Histories")}));
    }

    /// The form's read-status picker can master a book outright — that counts as
    /// an actual read (counters + journal), never double-counting.
    void formMasteryCountsAsRead()
    {
        BookDraft mastered;
        mastered.title = QStringLiteral("Low Mystery");
        mastered.readStatus = ReadStatus::Mastered;
        BookDraft catalogued;
        catalogued.title = QStringLiteral("Low Mystery");
        catalogued.readStatus = ReadStatus::Catalogued;

        QVERIFY(BookReadTransitions::countsAsRead(nullptr, mastered)); // created as mastered = one read
        QVERIFY(!BookReadTransitions::countsAsRead(nullptr, catalogued));

        const Book original = book(QStringLiteral("Low Mystery"), 1, {}, {}, {}, ReadStatus::Catalogued);
        const Book alreadyMastered = book(QStringLiteral("Low Mystery"), 2, {}, {}, {}, ReadStatus::Mastered);
        QVERIFY(BookReadTransitions::countsAsRead(&original, mastered));
        QVERIFY(!BookReadTransitions::countsAsRead(&alreadyMastered, mastered));
        QVERIFY(!BookReadTransitions::countsAsRead(&original, catalogued));
    }

    // MARK: Search

    void searchMatchesTitleCaseInsensitively()
    {
        QCOMPARE(apply(options(QStringLiteral("turquoise"))),
                 (QStringList{QStringLiteral("The Turquoise Hand")}));
        QCOMPARE(apply(options(QStringLiteral("ANNALS"))),
                 (QStringList{QStringLiteral("Annals of St Brandans")}));
    }

    void searchMatchesSetNameNotesPrincipleNameAndLanguageName()
    {
        QCOMPARE(apply(options(QStringLiteral("numen"))),
                 (QStringList{QStringLiteral("The Turquoise Hand")}));
        QCOMPARE(apply(options(QStringLiteral("persistent"))),
                 (QStringList{QStringLiteral("The Turquoise Hand")}));
        QCOMPARE(apply(options(QStringLiteral("rose"))),
                 (QStringList{QStringLiteral("The Turquoise Hand")}));
        QCOMPARE(apply(options(QStringLiteral("fucine"))),
                 (QStringList{QStringLiteral("The Turquoise Hand")}));
    }

    void emptyAndWhitespaceSearchReturnsEverything()
    {
        QCOMPARE(apply(options()).size(), 4);
        QCOMPARE(apply(options(QStringLiteral("   "))).size(), 4);
        QCOMPARE(apply(options(QStringLiteral("nothing matches this"))).size(), 0);
    }

    // MARK: Filters

    void statusFilters()
    {
        QCOMPARE(apply(options({}, BookStatusFilter::Unread)),
                 (QStringList{QStringLiteral("An Introduction to Histories"), QStringLiteral("De Bellis Murorum"),
                              QStringLiteral("The Turquoise Hand")}));
        QCOMPARE(apply(options({}, BookStatusFilter::Uncatalogued)),
                 (QStringList{QStringLiteral("An Introduction to Histories"), QStringLiteral("De Bellis Murorum")}));
        QCOMPARE(apply(options({}, BookStatusFilter::Catalogued)),
                 (QStringList{QStringLiteral("The Turquoise Hand")}));
        QCOMPARE(apply(options({}, BookStatusFilter::Mastered)),
                 (QStringList{QStringLiteral("Annals of St Brandans")}));
    }

    void contaminatedFilterExcludesClearAndUnknown()
    {
        QCOMPARE(apply(options({}, BookStatusFilter::Contaminated)),
                 (QStringList{QStringLiteral("De Bellis Murorum")}));
    }

    // MARK: Sorts

    void sortByTitleIsLocalized()
    {
        QCOMPARE(apply(options({}, BookStatusFilter::All, BookSort::Title)).first(),
                 QStringLiteral("An Introduction to Histories"));
    }

    void sortByDifficultyIsDescendingWithUnknownsLast()
    {
        QCOMPARE(apply(options({}, BookStatusFilter::All, BookSort::Difficulty)),
                 (QStringList{QStringLiteral("The Turquoise Hand"), QStringLiteral("De Bellis Murorum"),
                              QStringLiteral("Annals of St Brandans"),
                              QStringLiteral("An Introduction to Histories")}));
    }

    void sortByStatusGroupsUnreadFirst()
    {
        const QStringList sorted = apply(options({}, BookStatusFilter::All, BookSort::Status));
        QCOMPARE(sorted.last(), QStringLiteral("Annals of St Brandans")); // mastered last
        QCOMPARE(sorted.first(), QStringLiteral("An Introduction to Histories")); // unread first, ties alphabetical
    }

    void sortByRecency()
    {
        QCOMPARE(apply(options({}, BookStatusFilter::All, BookSort::Recent)).first(),
                 QStringLiteral("De Bellis Murorum"));
    }

    // MARK: ReadingLog

    void readingLogMasteringWithEverything()
    {
        const QString text = ReadingLog::journalText(
            book(QStringLiteral("The Turquoise Hand"), 1), true,
            QStringLiteral("Horizon-Sight"), QStringLiteral("Numen: That Old Lost Music"), 2,
            QStringLiteral("Found in the Silver Vault."));
        QCOMPARE(text,
                 QStringLiteral("Mastered “The Turquoise Hand”. Read with: Horizon-Sight. "
                                "Memory gained: Numen: That Old Lost Music. Lessons learned: 2. "
                                "Found in the Silver Vault."));
    }

    void readingLogReReadSkipsLessonsAndTrimsNote()
    {
        const QString text = ReadingLog::journalText(
            book(QStringLiteral("The Turquoise Hand"), 1), false, {},
            QStringLiteral("Numen: That Old Lost Music"), 2,
            QStringLiteral("   grinding memories for Numa   "));
        QCOMPARE(text,
                 QStringLiteral("Re-read “The Turquoise Hand”. Memory gained: Numen: That Old Lost Music. "
                                "grinding memories for Numa"));
        QVERIFY2(!text.contains(QStringLiteral("Lessons")), "re-reads don't grant lessons");
    }

    void readingLogEmptyOptionalsStaySilent()
    {
        const QString text = ReadingLog::journalText(book(QStringLiteral("Plain Book"), 1), true, {},
                                                     QString(), {}, {});
        QCOMPARE(text, QStringLiteral("Mastered “Plain Book”."));
    }
};

QTEST_MAIN(BookQueryTests)
#include "test_BookQuery.moc"
