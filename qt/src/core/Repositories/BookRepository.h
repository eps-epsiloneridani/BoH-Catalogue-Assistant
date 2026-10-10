// Port of BookRepository.swift — scoped to one playthrough: every list query
// filters by it and every insert stamps it (docs/DATABASE.md §Playthroughs).
// Id-based writes all carry WHERE playthrough_id = ? — fail-closed scoping.
#pragma once

#include "Models.h"
#include "SQLiteDatabase.h"

#include <QHash>
#include <QList>

#include <vector>

namespace boh {

class BookRepository {
public:
    struct LessonAmount {
        QString skillName;
        int amount = 0;
        bool operator==(const LessonAmount&) const = default;
    };

    BookRepository(SQLiteDatabase& db, qint64 playthroughID)
        : m_db(db), m_playthroughID(playthroughID)
    {
    }

    std::vector<Book> all() const;
    std::optional<Book> get(qint64 id) const;
    Book insert(const BookDraft& draft) const;
    /// Read counters (times_read, first/last_read_at) and playthrough_id are
    /// managed by recordRead()/updateReadStatus(), never by generic edits.
    void update(const Book& book) const;
    void remove(qint64 id) const;

    // MARK: Reading state

    void updateReadStatus(qint64 id, ReadStatus status) const;
    /// Log a read: increments the counter, stamps first/last read. Call
    /// updateReadStatus(id, Mastered) alongside it when the book was mastered.
    void recordRead(qint64 id) const;
    /// Point the book at the memory it always yields (record-read flow).
    void setYieldedMemory(qint64 id, std::optional<qint64> memoryID) const;
    /// Record how many Lessons the book granted on its mastering read.
    void setLessonsCount(qint64 id, std::optional<int> lessons) const;

    // MARK: Lessons junction

    /// Lesson junction rows grouped per book, skill names resolved in one query
    /// (scoped via the Books join) — feeds the book pane's live lesson cache.
    QHash<qint64, QList<LessonAmount>> lessonSkillAmountsByBook() const;
    std::vector<BookLessonsEntry> lessons(qint64 bookID) const;
    void setLessons(qint64 bookID, const std::vector<BookLessonsEntry>& entries) const;

    static Book mapRow(const Row& row);

private:
    SQLiteDatabase& m_db;
    qint64 m_playthroughID;
};

} // namespace boh
