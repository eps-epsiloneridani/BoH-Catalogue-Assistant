// Port of BookQuery.swift — client-side list queries for the Books screen.
// Kept in Core as pure functions so they're unit-testable.
#pragma once

#include "Models.h"

#include <QHash>
#include <QString>
#include <QtGlobal>

#include <optional>
#include <vector>

namespace boh {

enum class BookStatusFilter { All, Unread, Uncatalogued, Catalogued, Mastered, Contaminated };
enum class BookSort { Title, Difficulty, Status, Easiest, Recent };

struct BookQueryOptions {
    QString searchText;
    BookStatusFilter statusFilter = BookStatusFilter::All;
    /// Show only books whose mystery is THIS principle (Reading Helper's
    /// mystery filter); nullopt = any. Books with no recorded principle are
    /// excluded while a filter is active.
    std::optional<qint64> mysteryPrincipleID;
    BookSort sort = BookSort::Title;
};

class BookFiltering {
public:
    /// Filter + sort `books` for display. `principleNames`/`languageNames` let the
    /// search match the *names* a book's IDs point at, not just raw columns.
    static std::vector<Book> apply(std::vector<Book> books, const BookQueryOptions& options,
                                   const QHash<qint64, QString>& principleNames = {},
                                   const QHash<qint64, QString>& languageNames = {});
};

/// The book form (and the detail's status picker) can set a book straight to
/// mastered — quite possible in the game: catalogue a low-level mystery and
/// beat it immediately. A form master is an actual read: it logs the read
/// counters/stamps and a journal entry exactly like the record-read flow's
/// minimal path (memory/lessons capture stays with the record-read sheet).
class BookReadTransitions {
public:
    /// True when saving `draft` moves the book into mastered from another
    /// status (or creates it as mastered); an already-mastered book saved as
    /// mastered counts nothing further. `original` may be null (creation).
    static bool countsAsRead(const Book* original, const BookDraft& draft);
};

/// Composes the Journal entry written by the record-a-read flow. Pure and tested:
/// the wording is user-facing, so changes show up in git as test diffs.
class ReadingLog {
public:
    static QString journalText(const Book& book, bool mastering,
                               std::optional<QString> usedMemoryName,
                               std::optional<QString> gainedMemoryName,
                               std::optional<int> lessons, std::optional<QString> userNote);
};

} // namespace boh
