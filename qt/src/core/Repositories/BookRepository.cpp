#include "BookRepository.h"

namespace boh {

std::vector<Book> BookRepository::all() const
{
    return m_db.query<Book>(QStringLiteral("SELECT * FROM Books WHERE playthrough_id = ? ORDER BY title;"),
                            {SQLiteValue(m_playthroughID)},
                            [](const Row& row) { return mapRow(row); });
}

std::optional<Book> BookRepository::get(qint64 id) const
{
    const auto rows = m_db.query<Book>(
        QStringLiteral("SELECT * FROM Books WHERE playthrough_id = ? AND id = ?;"),
        {SQLiteValue(m_playthroughID), SQLiteValue(id)},
        [](const Row& row) { return mapRow(row); });
    return rows.empty() ? std::nullopt : std::optional<Book>(rows.front());
}

Book BookRepository::insert(const BookDraft& draft) const
{
    m_db.execute(QStringLiteral(
                     "INSERT INTO Books (title, set_name, volume, book_kind, language_id,"
                     "                   mystery_principle_id, difficulty, read_status,"
                     "                   contamination, location, lessons, yielded_memory_id,"
                     "                   notes, playthrough_id)"
                     " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"),
                 {SQLiteValue(draft.title), sv(draft.setName), sv(draft.volume),
                  SQLiteValue(bookKindToString(draft.bookKind)), sv(draft.languageID),
                  sv(draft.mysteryPrincipleID), sv(draft.difficulty),
                  SQLiteValue(readStatusToString(draft.readStatus)),
                  draft.contamination ? SQLiteValue(contaminationToString(*draft.contamination))
                                      : SQLiteValue(),
                  sv(draft.location), sv(draft.lessons), sv(draft.yieldedMemoryID), sv(draft.notes),
                  SQLiteValue(m_playthroughID)});
    return *get(m_db.lastInsertRowID());
}

void BookRepository::update(const Book& book) const
{
    m_db.execute(QStringLiteral(
                     "UPDATE Books"
                     " SET title = ?, set_name = ?, volume = ?, book_kind = ?, language_id = ?,"
                     "     mystery_principle_id = ?, difficulty = ?, read_status = ?,"
                     "     contamination = ?, location = ?, lessons = ?, yielded_memory_id = ?,"
                     "     notes = ?, updated_at = datetime('now')"
                     " WHERE playthrough_id = ? AND id = ?"),
                 {SQLiteValue(book.title), sv(book.setName), sv(book.volume),
                  SQLiteValue(bookKindToString(book.bookKind)), sv(book.languageID),
                  sv(book.mysteryPrincipleID), sv(book.difficulty),
                  SQLiteValue(readStatusToString(book.readStatus)),
                  book.contamination ? SQLiteValue(contaminationToString(*book.contamination))
                                     : SQLiteValue(),
                  sv(book.location), sv(book.lessons), sv(book.yieldedMemoryID), sv(book.notes),
                  SQLiteValue(m_playthroughID), SQLiteValue(book.id)});
}

void BookRepository::remove(qint64 id) const
{
    m_db.execute(QStringLiteral("DELETE FROM Books WHERE playthrough_id = ? AND id = ?;"),
                 {SQLiteValue(m_playthroughID), SQLiteValue(id)});
}

void BookRepository::updateReadStatus(qint64 id, ReadStatus status) const
{
    m_db.execute(QStringLiteral(
                     "UPDATE Books SET read_status = ?, updated_at = datetime('now')"
                     " WHERE playthrough_id = ? AND id = ?"),
                 {SQLiteValue(readStatusToString(status)), SQLiteValue(m_playthroughID),
                  SQLiteValue(id)});
}

void BookRepository::recordRead(qint64 id) const
{
    m_db.execute(QStringLiteral(
                     "UPDATE Books"
                     " SET times_read = times_read + 1,"
                     "     first_read_at = COALESCE(first_read_at, datetime('now')),"
                     "     last_read_at = datetime('now'),"
                     "     updated_at = datetime('now')"
                     " WHERE playthrough_id = ? AND id = ?;"),
                 {SQLiteValue(m_playthroughID), SQLiteValue(id)});
}

void BookRepository::setYieldedMemory(qint64 id, std::optional<qint64> memoryID) const
{
    m_db.execute(QStringLiteral(
                     "UPDATE Books SET yielded_memory_id = ?, updated_at = datetime('now')"
                     " WHERE playthrough_id = ? AND id = ?"),
                 {sv(memoryID), SQLiteValue(m_playthroughID), SQLiteValue(id)});
}

void BookRepository::setLessonsCount(qint64 id, std::optional<int> lessons) const
{
    m_db.execute(QStringLiteral(
                     "UPDATE Books SET lessons = ?, updated_at = datetime('now')"
                     " WHERE playthrough_id = ? AND id = ?"),
                 {sv(lessons), SQLiteValue(m_playthroughID), SQLiteValue(id)});
}

QHash<qint64, QList<BookRepository::LessonAmount>> BookRepository::lessonSkillAmountsByBook() const
{
    const auto rows = m_db.query<std::tuple<qint64, QString, int>>(
        QStringLiteral(
            "SELECT bl.book_id, s.name AS skill_name, bl.amount"
            " FROM BookLessons bl"
            " JOIN Books b ON b.id = bl.book_id"
            " JOIN Skills s ON s.id = bl.skill_id"
            " WHERE b.playthrough_id = ?;"),
        {SQLiteValue(m_playthroughID)},
        [](const Row& row) {
            return std::make_tuple(row.requireInt64(QStringLiteral("book_id")),
                                   row.requireString(QStringLiteral("skill_name")),
                                   row.requireInteger(QStringLiteral("amount")));
        });
    QHash<qint64, QList<LessonAmount>> grouped;
    for (const auto& [bookID, skillName, amount] : rows)
        grouped[bookID].append(LessonAmount{skillName, amount});
    return grouped;
}

std::vector<BookLessonsEntry> BookRepository::lessons(qint64 bookID) const
{
    return m_db.query<BookLessonsEntry>(
        QStringLiteral("SELECT skill_id, amount FROM BookLessons WHERE book_id = ?;"),
        {SQLiteValue(bookID)},
        [](const Row& row) {
            return BookLessonsEntry{row.requireInt64(QStringLiteral("skill_id")),
                                    row.requireInteger(QStringLiteral("amount"))};
        });
}

void BookRepository::setLessons(qint64 bookID, const std::vector<BookLessonsEntry>& entries) const
{
    m_db.transaction([&] {
        m_db.execute(QStringLiteral("DELETE FROM BookLessons WHERE book_id = ?;"),
                     {SQLiteValue(bookID)});
        for (const BookLessonsEntry& entry : entries) {
            m_db.execute(QStringLiteral(
                             "INSERT INTO BookLessons (book_id, skill_id, amount) VALUES (?, ?, ?);"),
                         {SQLiteValue(bookID), SQLiteValue(entry.skillID), SQLiteValue(entry.amount)});
        }
    });
}

Book BookRepository::mapRow(const Row& row)
{
    Book b;
    b.id = row.requireInt64(QStringLiteral("id"));
    b.title = row.requireString(QStringLiteral("title"));
    b.setName = row.string(QStringLiteral("set_name"));
    b.volume = row.string(QStringLiteral("volume"));
    b.bookKind = bookKindFromString(row.requireString(QStringLiteral("book_kind")))
                     .value_or(BookKind::Book);
    b.languageID = row.int64(QStringLiteral("language_id"));
    b.mysteryPrincipleID = row.int64(QStringLiteral("mystery_principle_id"));
    b.difficulty = row.integer(QStringLiteral("difficulty"));
    b.readStatus = readStatusFromString(row.requireString(QStringLiteral("read_status")))
                       .value_or(ReadStatus::Uncatalogued);
    if (const auto raw = row.string(QStringLiteral("contamination")))
        b.contamination = contaminationFromString(*raw);
    b.location = row.string(QStringLiteral("location"));
    b.timesRead = row.requireInteger(QStringLiteral("times_read"));
    b.firstReadAt = row.string(QStringLiteral("first_read_at"));
    b.lastReadAt = row.string(QStringLiteral("last_read_at"));
    b.lessons = row.integer(QStringLiteral("lessons"));
    b.yieldedMemoryID = row.int64(QStringLiteral("yielded_memory_id"));
    b.notes = row.string(QStringLiteral("notes"));
    return b;
}

} // namespace boh
