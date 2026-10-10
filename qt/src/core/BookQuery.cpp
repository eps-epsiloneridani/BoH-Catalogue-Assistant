#include "BookQuery.h"

#include <QCollator>
#include <QStringList>

#include <algorithm>
#include <climits>

namespace boh {

namespace {
/// localizedStandardCompare analog: locale-aware with numeric mode ("vol. 2"
/// sorts before "vol. 10"). Main-thread only, like all db work.
bool titleLess(const QString& a, const QString& b)
{
    static const QCollator collator = [] {
        QCollator c;
        c.setNumericMode(true);
        return c;
    }();
    return collator.compare(a, b) < 0;
}

int readRank(const Book& book)
{
    switch (book.readStatus) {
    case ReadStatus::Uncatalogued: return 0;
    case ReadStatus::Catalogued: return 1;
    case ReadStatus::Mastered: return 2;
    }
    return 0;
}

template <typename RankFn>
void sortByRankThenTitle(std::vector<Book>& books, RankFn rank)
{
    std::stable_sort(books.begin(), books.end(), [&](const Book& a, const Book& b) {
        const int ra = rank(a);
        const int rb = rank(b);
        return ra != rb ? ra < rb : titleLess(a.title, b.title);
    });
}
} // namespace

std::vector<Book> BookFiltering::apply(std::vector<Book> books, const BookQueryOptions& options,
                                       const QHash<qint64, QString>& principleNames,
                                       const QHash<qint64, QString>& languageNames)
{
    std::vector<Book>& result = books;

    // Search: case-insensitive substring across the visible text a player knows.
    const QString query = options.searchText.trimmed();
    if (!query.isEmpty()) {
        const QString needle = query.toLower();
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [&](const Book& book) {
                                        QStringList parts;
                                        parts << book.title << book.setName.value_or(QString())
                                              << book.volume.value_or(QString())
                                              << book.location.value_or(QString())
                                              << book.notes.value_or(QString())
                                              << (book.mysteryPrincipleID
                                                      ? principleNames.value(*book.mysteryPrincipleID)
                                                      : QString())
                                              << (book.languageID ? languageNames.value(*book.languageID)
                                                                  : QString())
                                              << (book.difficulty ? QString::number(*book.difficulty)
                                                                  : QString());
                                        return !parts.join(QLatin1Char(' ')).toLower().contains(needle);
                                    }),
                    result.end());
    }

    switch (options.statusFilter) {
    case BookStatusFilter::All:
        break;
    case BookStatusFilter::Unread:
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [](const Book& b) { return b.readStatus == ReadStatus::Mastered; }),
                     result.end());
        break;
    case BookStatusFilter::Uncatalogued:
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [](const Book& b) {
                                        return b.readStatus != ReadStatus::Uncatalogued;
                                    }),
                     result.end());
        break;
    case BookStatusFilter::Catalogued:
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [](const Book& b) {
                                        return b.readStatus != ReadStatus::Catalogued;
                                    }),
                     result.end());
        break;
    case BookStatusFilter::Mastered:
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [](const Book& b) { return b.readStatus != ReadStatus::Mastered; }),
                     result.end());
        break;
    case BookStatusFilter::Contaminated:
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [](const Book& b) {
                                        return !b.contamination.has_value()
                                               || *b.contamination == Contamination::None;
                                    }),
                     result.end());
        break;
    }

    // Mystery filter (Reading Helper + any screen adopting it).
    if (options.mysteryPrincipleID) {
        const qint64 principleID = *options.mysteryPrincipleID;
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [&](const Book& b) { return b.mysteryPrincipleID != principleID; }),
                     result.end());
    }

    switch (options.sort) {
    case BookSort::Title:
        std::stable_sort(result.begin(), result.end(),
                         [](const Book& a, const Book& b) { return titleLess(a.title, b.title); });
        break;
    case BookSort::Easiest:
        // Reading-planning order: lowest difficulty first, unknown last.
        sortByRankThenTitle(result, [](const Book& b) { return b.difficulty.value_or(INT_MAX); });
        break;
    case BookSort::Difficulty:
        // Descending difficulty, unknowns after every known value.
        std::stable_sort(result.begin(), result.end(), [](const Book& a, const Book& b) {
            if (a.difficulty.has_value() != b.difficulty.has_value())
                return a.difficulty.has_value();
            if (a.difficulty && *a.difficulty != *b.difficulty)
                return *a.difficulty > *b.difficulty;
            return titleLess(a.title, b.title);
        });
        break;
    case BookSort::Status:
        sortByRankThenTitle(result, [](const Book& b) { return readRank(b); });
        break;
    case BookSort::Recent:
        std::stable_sort(result.begin(), result.end(),
                         [](const Book& a, const Book& b) { return a.id > b.id; });
        break;
    }

    return result;
}

bool BookReadTransitions::countsAsRead(const Book* original, const BookDraft& draft)
{
    return draft.readStatus == ReadStatus::Mastered
           && (!original || original->readStatus != ReadStatus::Mastered);
}

QString ReadingLog::journalText(const Book& book, bool mastering,
                                std::optional<QString> usedMemoryName,
                                std::optional<QString> gainedMemoryName,
                                std::optional<int> lessons, std::optional<QString> userNote)
{
    QStringList sentences;
    sentences << (mastering ? QStringLiteral("Mastered “%1”.").arg(book.title)
                            : QStringLiteral("Re-read “%1”.").arg(book.title));
    if (usedMemoryName && !usedMemoryName->isEmpty())
        sentences << QStringLiteral("Read with: %1.").arg(*usedMemoryName);
    if (gainedMemoryName && !gainedMemoryName->isEmpty())
        sentences << QStringLiteral("Memory gained: %1.").arg(*gainedMemoryName);
    if (mastering && lessons)
        sentences << QStringLiteral("Lessons learned: %1.").arg(*lessons);
    if (userNote && !userNote->trimmed().isEmpty())
        sentences << userNote->trimmed();
    return sentences.join(QLatin1Char(' '));
}

} // namespace boh
