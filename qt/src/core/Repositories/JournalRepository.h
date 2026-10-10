// Port of JournalRepository.swift (playthrough-scoped, fail-closed writes).
#pragma once

#include "Models.h"
#include "SQLiteDatabase.h"

#include <QHash>
#include <QList>

namespace boh {

class JournalRepository {
public:
    JournalRepository(SQLiteDatabase& db, qint64 playthroughID)
        : m_db(db), m_playthroughID(playthroughID)
    {
    }

    /// Newest first.
    std::vector<JournalEntry> recent(int limit = 100) const;
    std::optional<JournalEntry> get(qint64 id) const;
    /// All book-linked entries grouped per book in one query (newest first per
    /// book) — the book pane reads live caches built from this.
    QHash<qint64, QList<JournalEntry>> entriesByBook() const;
    /// Entries linked to a given entity, newest first.
    std::vector<JournalEntry> entries(std::optional<qint64> bookID = {},
                                      std::optional<qint64> memoryID = {},
                                      std::optional<qint64> skillID = {}, int limit = 100) const;
    JournalEntry insert(const JournalDraft& draft) const;
    void update(const JournalEntry& entry) const;
    void remove(qint64 id) const;

    static JournalEntry mapRow(const Row& row);

private:
    SQLiteDatabase& m_db;
    qint64 m_playthroughID;
};

} // namespace boh
