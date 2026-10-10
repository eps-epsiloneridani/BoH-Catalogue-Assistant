// Port of MemoryRepository.swift. The earned-visibility spoiler posture and
// insertOrReuse entity reuse are the behaviors most under test — ported
// deliberately, not "simplified".
#pragma once

#include "Models.h"
#include "SQLiteDatabase.h"

#include <QHash>
#include <QList>

#include <vector>

namespace boh {

class MemoryRepository {
public:
    MemoryRepository(SQLiteDatabase& db, qint64 playthroughID)
        : m_db(db), m_playthroughID(playthroughID)
    {
    }

    // MARK: CRUD

    std::vector<Memory> all() const;
    /// The memories the player can actually know: earned-only (see earnedVisibility).
    std::vector<Memory> allKnown() const;
    std::optional<Memory> get(qint64 id) const;
    Memory insert(const MemoryDraft& draft) const;
    /// Insert `draft`, unless a memory of the same (name, kind) already exists in
    /// this playthrough — reusing links the existing row rather than failing the
    /// whole read (import seeds hidden yields a record-read legitimately re-creates).
    /// Case-insensitive via QString (Unicode-aware, unlike SQL LOWER).
    Memory insertOrReuse(const MemoryDraft& draft) const;
    void update(const Memory& memory) const;
    void remove(qint64 id) const;

    /// Earned-visibility SQL over alias `m` (also drives the Reading Helper's
    /// aspect candidates — docs/DATABASE.md §Memories).
    static const char* earnedVisibility;

    // MARK: Aspects

    std::vector<Aspect> aspects(qint64 memoryID) const;
    /// Replace the memory's aspects entirely.
    void setAspects(qint64 memoryID, const std::vector<AspectDraft>& aspects) const;

    // MARK: Sources

    std::vector<MemorySource> sources(qint64 memoryID) const;
    void addSource(qint64 memoryID, const QString& kind, std::optional<QString> detail = {}) const;
    /// Replace the memory's "how to obtain" rows wholesale (form save).
    void setSources(qint64 memoryID, const std::vector<MemorySource>& sources) const;
    void removeSource(qint64 memoryID, const QString& kind, std::optional<QString> detail = {}) const;

    /// All "how to obtain" rows grouped per memory in one query — the UI caches
    /// live from this after every reload rather than snapshots that go stale.
    QHash<qint64, QList<MemorySource>> allSourcesByMemory() const;
    /// Mastered-only yielding links grouped per memory (display semantics).
    QHash<qint64, QList<BookRef>> yieldingByMemory() const;
    /// Every book currently linked as yielding this memory, any read status —
    /// for editing; the mastered-only booksYielding() is for display.
    std::vector<BookRef> allYielding(qint64 memoryID) const;
    /// Sync the yielding links to `bookIDs` (form save): books linked but not
    /// listed get unlinked, listed books get the link. Same-playthrough only;
    /// ids are bound, never interpolated.
    void setYieldingBooks(qint64 memoryID, const std::vector<qint64>& bookIDs) const;

    // MARK: Reading Helper (canonical query 1)

    /// Earned memories whose level in `principleID` is at least `minLevel`, best first.
    std::vector<MemoryCandidate> candidates(qint64 principleID, int minLevel) const;
    /// Books that yield this memory when read — mastered-only (spoiler policy).
    std::vector<BookRef> booksYielding(qint64 memoryID) const;

    static Memory mapRow(const Row& row);
    static Aspect mapAspectRow(const Row& row);

private:
    std::vector<Memory> attachAspects(std::vector<Memory> memories) const;
    qint64 insertRow(const MemoryDraft& draft) const;

    SQLiteDatabase& m_db;
    qint64 m_playthroughID;
};

} // namespace boh
