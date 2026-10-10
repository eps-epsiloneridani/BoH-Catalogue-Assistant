#include "JournalRepository.h"

#include <QStringList>

namespace boh {

std::vector<JournalEntry> JournalRepository::recent(int limit) const
{
    return m_db.query<JournalEntry>(
        QStringLiteral(
            "SELECT * FROM Journal WHERE playthrough_id = ? ORDER BY logged_at DESC, id DESC LIMIT ?;"),
        {SQLiteValue(m_playthroughID), SQLiteValue(limit)},
        [](const Row& row) { return mapRow(row); });
}

std::optional<JournalEntry> JournalRepository::get(qint64 id) const
{
    const auto rows = m_db.query<JournalEntry>(
        QStringLiteral("SELECT * FROM Journal WHERE playthrough_id = ? AND id = ?;"),
        {SQLiteValue(m_playthroughID), SQLiteValue(id)},
        [](const Row& row) { return mapRow(row); });
    return rows.empty() ? std::nullopt : std::optional<JournalEntry>(rows.front());
}

QHash<qint64, QList<JournalEntry>> JournalRepository::entriesByBook() const
{
    const auto rows = m_db.query<JournalEntry>(
        QStringLiteral("SELECT * FROM Journal"
                       " WHERE playthrough_id = ? AND book_id IS NOT NULL"
                       " ORDER BY logged_at DESC, id DESC;"),
        {SQLiteValue(m_playthroughID)}, [](const Row& row) { return mapRow(row); });
    QHash<qint64, QList<JournalEntry>> grouped;
    for (const JournalEntry& entry : rows) {
        if (entry.bookID)
            grouped[*entry.bookID].append(entry);
    }
    return grouped;
}

std::vector<JournalEntry> JournalRepository::entries(std::optional<qint64> bookID,
                                                     std::optional<qint64> memoryID,
                                                     std::optional<qint64> skillID, int limit) const
{
    QStringList clauses {QStringLiteral("playthrough_id = ?")};
    std::vector<SQLiteValue> binds {SQLiteValue(m_playthroughID)};
    if (bookID) {
        clauses << QStringLiteral("book_id = ?");
        binds.emplace_back(*bookID);
    }
    if (memoryID) {
        clauses << QStringLiteral("memory_id = ?");
        binds.emplace_back(*memoryID);
    }
    if (skillID) {
        clauses << QStringLiteral("skill_id = ?");
        binds.emplace_back(*skillID);
    }
    binds.emplace_back(limit);
    return m_db.query<JournalEntry>(
        QStringLiteral("SELECT * FROM Journal WHERE %1 ORDER BY logged_at DESC, id DESC LIMIT ?;")
            .arg(clauses.join(QStringLiteral(" AND "))),
        binds, [](const Row& row) { return mapRow(row); });
}

JournalEntry JournalRepository::insert(const JournalDraft& draft) const
{
    m_db.execute(QStringLiteral(
                     "INSERT INTO Journal (game_day, entry, book_id, memory_id, skill_id, playthrough_id)"
                     " VALUES (?, ?, ?, ?, ?, ?);"),
                 {sv(draft.gameDay), SQLiteValue(draft.entry), sv(draft.bookID), sv(draft.memoryID),
                  sv(draft.skillID), SQLiteValue(m_playthroughID)});
    return *get(m_db.lastInsertRowID());
}

void JournalRepository::update(const JournalEntry& entry) const
{
    m_db.execute(QStringLiteral(
                     "UPDATE Journal"
                     " SET game_day = ?, entry = ?, book_id = ?, memory_id = ?, skill_id = ?"
                     " WHERE playthrough_id = ? AND id = ?"),
                 {sv(entry.gameDay), SQLiteValue(entry.entry), sv(entry.bookID), sv(entry.memoryID),
                  sv(entry.skillID), SQLiteValue(m_playthroughID), SQLiteValue(entry.id)});
}

void JournalRepository::remove(qint64 id) const
{
    m_db.execute(QStringLiteral("DELETE FROM Journal WHERE playthrough_id = ? AND id = ?;"),
                 {SQLiteValue(m_playthroughID), SQLiteValue(id)});
}

JournalEntry JournalRepository::mapRow(const Row& row)
{
    JournalEntry e;
    e.id = row.requireInt64(QStringLiteral("id"));
    e.loggedAt = row.requireString(QStringLiteral("logged_at"));
    e.gameDay = row.string(QStringLiteral("game_day"));
    e.entry = row.requireString(QStringLiteral("entry"));
    e.bookID = row.int64(QStringLiteral("book_id"));
    e.memoryID = row.int64(QStringLiteral("memory_id"));
    e.skillID = row.int64(QStringLiteral("skill_id"));
    return e;
}

} // namespace boh
