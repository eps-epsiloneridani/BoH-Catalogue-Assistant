#include "MemoryRepository.h"

#include <QStringList>

#include <algorithm>

namespace boh {

// Earned-visibility SQL over alias `m`: a memory is visible when it has no yield
// links (hand-created) or at least one yielding book is mastered (docs/DATABASE.md).
const char* MemoryRepository::earnedVisibility =
    "( NOT EXISTS ("
    "    SELECT 1 FROM Books b"
    "    WHERE b.playthrough_id = m.playthrough_id"
    "      AND b.yielded_memory_id = m.id )"
    "  OR EXISTS ("
    "    SELECT 1 FROM Books b"
    "    WHERE b.playthrough_id = m.playthrough_id"
    "      AND b.yielded_memory_id = m.id"
    "      AND b.read_status = 'mastered' ) )";

std::vector<Memory> MemoryRepository::all() const
{
    return attachAspects(m_db.query<Memory>(
        QStringLiteral("SELECT * FROM Memories WHERE playthrough_id = ? ORDER BY name;"),
        {SQLiteValue(m_playthroughID)}, [](const Row& row) { return mapRow(row); }));
}

std::vector<Memory> MemoryRepository::allKnown() const
{
    return attachAspects(m_db.query<Memory>(
        QStringLiteral("SELECT * FROM Memories m"
                       " WHERE m.playthrough_id = ? AND %1"
                       " ORDER BY name;")
            .arg(QLatin1String(earnedVisibility)),
        {SQLiteValue(m_playthroughID)}, [](const Row& row) { return mapRow(row); }));
}

std::optional<Memory> MemoryRepository::get(qint64 id) const
{
    const auto rows = m_db.query<Memory>(
        QStringLiteral("SELECT * FROM Memories WHERE playthrough_id = ? AND id = ?;"),
        {SQLiteValue(m_playthroughID), SQLiteValue(id)}, [](const Row& row) { return mapRow(row); });
    if (rows.empty())
        return std::nullopt;
    Memory memory = rows.front();
    memory.aspects = aspects(id);
    return memory;
}

Memory MemoryRepository::insert(const MemoryDraft& draft) const
{
    const qint64 id = insertRow(draft);
    setAspects(id, draft.aspects);
    return *get(id);
}

Memory MemoryRepository::insertOrReuse(const MemoryDraft& draft) const
{
    // Trim the name: otherwise a trailing-space draft bypasses the
    // case-insensitive match AND the raw UNIQUE, creating a duplicate.
    const QString name = draft.name.trimmed();
    const auto rows = m_db.query<std::pair<qint64, QString>>(
        QStringLiteral("SELECT id, name FROM Memories"
                       " WHERE playthrough_id = ? AND kind = ?;"),
        {SQLiteValue(m_playthroughID), SQLiteValue(memoryKindToString(draft.kind))},
        [](const Row& row) {
            return std::make_pair(row.requireInt64(QStringLiteral("id")),
                                  row.requireString(QStringLiteral("name")));
        });
    const auto match = std::find_if(rows.begin(), rows.end(), [&](const auto& idAndName) {
        // QString case-insensitive compare: Unicode-aware (SQL LOWER is ASCII-only,
        // so accented entity names would split into near-duplicates).
        return idAndName.second.compare(name, Qt::CaseInsensitive) == 0;
    });
    if (match != rows.end())
        return *get(match->first);
    MemoryDraft trimmedDraft = draft;
    trimmedDraft.name = name;
    return insert(trimmedDraft);
}

void MemoryRepository::update(const Memory& memory) const
{
    m_db.transaction([&] {
        m_db.execute(QStringLiteral(
                         "UPDATE Memories"
                         " SET name = ?, kind = ?, persistent = ?, notes = ?, updated_at = datetime('now')"
                         " WHERE playthrough_id = ? AND id = ?"),
                     {SQLiteValue(memory.name), SQLiteValue(memoryKindToString(memory.kind)),
                      SQLiteValue(memory.persistent), sv(memory.notes),
                      SQLiteValue(m_playthroughID), SQLiteValue(memory.id)});
        std::vector<AspectDraft> drafts;
        drafts.reserve(memory.aspects.size());
        for (const Aspect& aspect : memory.aspects)
            drafts.push_back(AspectDraft{aspect.principleID, aspect.level});
        setAspects(memory.id, drafts);
    });
}

void MemoryRepository::remove(qint64 id) const
{
    m_db.execute(QStringLiteral("DELETE FROM Memories WHERE playthrough_id = ? AND id = ?;"),
                 {SQLiteValue(m_playthroughID), SQLiteValue(id)});
}

qint64 MemoryRepository::insertRow(const MemoryDraft& draft) const
{
    m_db.execute(QStringLiteral(
                     "INSERT INTO Memories (name, kind, persistent, notes, playthrough_id)"
                     " VALUES (?, ?, ?, ?, ?);"),
                 {SQLiteValue(draft.name), SQLiteValue(memoryKindToString(draft.kind)),
                  SQLiteValue(draft.persistent), sv(draft.notes), SQLiteValue(m_playthroughID)});
    return m_db.lastInsertRowID();
}

std::vector<Aspect> MemoryRepository::aspects(qint64 memoryID) const
{
    return m_db.query<Aspect>(
        QStringLiteral(
            "SELECT ma.principle_id, p.name AS principle_name, ma.level"
            " FROM MemoryAspects ma"
            " JOIN Principles p ON p.id = ma.principle_id"
            " WHERE ma.memory_id = ?"
            " ORDER BY p.sort_order;"),
        {SQLiteValue(memoryID)}, [](const Row& row) { return mapAspectRow(row); });
}

void MemoryRepository::setAspects(qint64 memoryID, const std::vector<AspectDraft>& aspects) const
{
    m_db.transaction([&] {
        m_db.execute(QStringLiteral("DELETE FROM MemoryAspects WHERE memory_id = ?;"),
                     {SQLiteValue(memoryID)});
        for (const AspectDraft& aspect : aspects) {
            m_db.execute(QStringLiteral(
                             "INSERT INTO MemoryAspects (memory_id, principle_id, level) VALUES (?, ?, ?);"),
                         {SQLiteValue(memoryID), SQLiteValue(aspect.principleID),
                          SQLiteValue(aspect.level)});
        }
    });
}

std::vector<MemorySource> MemoryRepository::sources(qint64 memoryID) const
{
    return m_db.query<MemorySource>(
        QStringLiteral("SELECT kind, detail FROM MemorySources WHERE memory_id = ? ORDER BY kind, detail;"),
        {SQLiteValue(memoryID)},
        [](const Row& row) {
            return MemorySource{row.requireString(QStringLiteral("kind")),
                                row.string(QStringLiteral("detail"))};
        });
}

void MemoryRepository::addSource(qint64 memoryID, const QString& kind,
                                 std::optional<QString> detail) const
{
    m_db.execute(QStringLiteral(
                     "INSERT OR IGNORE INTO MemorySources (memory_id, kind, detail) VALUES (?, ?, ?);"),
                 {SQLiteValue(memoryID), SQLiteValue(kind), sv(detail)});
}

void MemoryRepository::setSources(qint64 memoryID, const std::vector<MemorySource>& sources) const
{
    m_db.transaction([&] {
        m_db.execute(QStringLiteral("DELETE FROM MemorySources WHERE memory_id = ?;"),
                     {SQLiteValue(memoryID)});
        for (const MemorySource& source : sources) {
            m_db.execute(QStringLiteral(
                             "INSERT OR IGNORE INTO MemorySources (memory_id, kind, detail) VALUES (?, ?, ?);"),
                         {SQLiteValue(memoryID), SQLiteValue(source.kind), sv(source.detail)});
        }
    });
}

void MemoryRepository::removeSource(qint64 memoryID, const QString& kind,
                                    std::optional<QString> detail) const
{
    m_db.execute(QStringLiteral(
                     "DELETE FROM MemorySources WHERE memory_id = ? AND kind = ? AND detail IS ?;"),
                 {SQLiteValue(memoryID), SQLiteValue(kind), sv(detail)});
}

QHash<qint64, QList<MemorySource>> MemoryRepository::allSourcesByMemory() const
{
    const auto rows = m_db.query<std::tuple<qint64, QString, std::optional<QString>>>(
        QStringLiteral(
            "SELECT ms.memory_id, ms.kind, ms.detail"
            " FROM MemorySources ms"
            " JOIN Memories m ON m.id = ms.memory_id"
            " WHERE m.playthrough_id = ?"
            " ORDER BY ms.kind, ms.detail;"),
        {SQLiteValue(m_playthroughID)},
        [](const Row& row) {
            return std::make_tuple(row.requireInt64(QStringLiteral("memory_id")),
                                   row.requireString(QStringLiteral("kind")),
                                   row.string(QStringLiteral("detail")));
        });
    QHash<qint64, QList<MemorySource>> grouped;
    for (const auto& [memoryID, kind, detail] : rows)
        grouped[memoryID].append(MemorySource{kind, detail});
    return grouped;
}

QHash<qint64, QList<BookRef>> MemoryRepository::yieldingByMemory() const
{
    const auto rows = m_db.query<std::tuple<qint64, QString, qint64>>(
        QStringLiteral(
            "SELECT id, title, yielded_memory_id FROM Books"
            " WHERE playthrough_id = ? AND yielded_memory_id IS NOT NULL"
            "   AND read_status = 'mastered'"
            " ORDER BY title;"),
        {SQLiteValue(m_playthroughID)},
        [](const Row& row) {
            return std::make_tuple(row.requireInt64(QStringLiteral("id")),
                                   row.requireString(QStringLiteral("title")),
                                   row.requireInt64(QStringLiteral("yielded_memory_id")));
        });
    QHash<qint64, QList<BookRef>> grouped;
    for (const auto& [id, title, memoryID] : rows)
        grouped[memoryID].append(BookRef{id, title});
    return grouped;
}

std::vector<BookRef> MemoryRepository::allYielding(qint64 memoryID) const
{
    return m_db.query<BookRef>(
        QStringLiteral("SELECT id, title FROM Books"
                       " WHERE playthrough_id = ? AND yielded_memory_id = ?"
                       " ORDER BY title;"),
        {SQLiteValue(m_playthroughID), SQLiteValue(memoryID)},
        [](const Row& row) {
            return BookRef{row.requireInt64(QStringLiteral("id")),
                           row.requireString(QStringLiteral("title"))};
        });
}

void MemoryRepository::setYieldingBooks(qint64 memoryID, const std::vector<qint64>& bookIDs) const
{
    m_db.transaction([&] {
        if (bookIDs.empty()) {
            m_db.execute(QStringLiteral(
                             "UPDATE Books SET yielded_memory_id = NULL, updated_at = datetime('now')"
                             " WHERE playthrough_id = ? AND yielded_memory_id = ?;"),
                         {SQLiteValue(m_playthroughID), SQLiteValue(memoryID)});
        } else {
            QStringList placeholders;
            for (std::size_t i = 0; i < bookIDs.size(); ++i)
                placeholders << QStringLiteral("?");
            std::vector<SQLiteValue> binds {SQLiteValue(m_playthroughID), SQLiteValue(memoryID)};
            for (qint64 id : bookIDs)
                binds.emplace_back(id);
            m_db.execute(QStringLiteral(
                             "UPDATE Books SET yielded_memory_id = NULL, updated_at = datetime('now')"
                             " WHERE playthrough_id = ? AND yielded_memory_id = ?"
                             "   AND id NOT IN (%1);")
                             .arg(placeholders.join(QStringLiteral(", "))),
                         binds);
        }
        for (qint64 id : bookIDs) {
            m_db.execute(QStringLiteral(
                             "UPDATE Books SET yielded_memory_id = ?, updated_at = datetime('now')"
                             " WHERE playthrough_id = ? AND id = ?;"),
                         {SQLiteValue(memoryID), SQLiteValue(m_playthroughID), SQLiteValue(id)});
        }
    });
}

std::vector<MemoryCandidate> MemoryRepository::candidates(qint64 principleID, int minLevel) const
{
    return m_db.query<MemoryCandidate>(
        QStringLiteral(
            "SELECT m.id, m.name, m.kind, m.persistent, ma.level"
            " FROM Memories m"
            " JOIN MemoryAspects ma ON ma.memory_id = m.id"
            " WHERE m.playthrough_id = ? AND ma.principle_id = ? AND ma.level >= ?"
            "   AND %1"
            " ORDER BY ma.level DESC, m.name;")
            .arg(QLatin1String(earnedVisibility)),
        {SQLiteValue(m_playthroughID), SQLiteValue(principleID), SQLiteValue(minLevel)},
        [](const Row& row) {
            MemoryCandidate candidate;
            candidate.id = row.requireInt64(QStringLiteral("id"));
            candidate.name = row.requireString(QStringLiteral("name"));
            candidate.kind = memoryKindFromString(row.requireString(QStringLiteral("kind")))
                                 .value_or(MemoryKind::Memory);
            candidate.persistent = row.boolean(QStringLiteral("persistent"));
            candidate.level = row.requireInteger(QStringLiteral("level"));
            return candidate;
        });
}

std::vector<BookRef> MemoryRepository::booksYielding(qint64 memoryID) const
{
    return m_db.query<BookRef>(
        QStringLiteral("SELECT id, title FROM Books"
                       " WHERE playthrough_id = ? AND yielded_memory_id = ?"
                       "   AND read_status = 'mastered'"
                       " ORDER BY title;"),
        {SQLiteValue(m_playthroughID), SQLiteValue(memoryID)},
        [](const Row& row) {
            return BookRef{row.requireInt64(QStringLiteral("id")),
                           row.requireString(QStringLiteral("title"))};
        });
}

std::vector<Memory> MemoryRepository::attachAspects(std::vector<Memory> memories) const
{
    if (memories.empty())
        return memories;
    const auto rows = m_db.query<std::pair<qint64, Aspect>>(
        QStringLiteral(
            "SELECT ma.memory_id, ma.principle_id, p.name AS principle_name, ma.level"
            " FROM MemoryAspects ma"
            " JOIN Principles p ON p.id = ma.principle_id"
            " ORDER BY p.sort_order;"),
        {},
        [](const Row& row) {
            return std::make_pair(row.requireInt64(QStringLiteral("memory_id")),
                                  mapAspectRow(row));
        });
    QHash<qint64, QList<Aspect>> byMemory;
    for (const auto& [memoryID, aspect] : rows)
        byMemory[memoryID].append(aspect);
    for (Memory& memory : memories) {
        memory.aspects.assign(byMemory.value(memory.id).cbegin(), byMemory.value(memory.id).cend());
    }
    return memories;
}

Aspect MemoryRepository::mapAspectRow(const Row& row)
{
    return Aspect{row.requireInt64(QStringLiteral("principle_id")),
                  row.requireString(QStringLiteral("principle_name")),
                  row.requireInteger(QStringLiteral("level"))};
}

Memory MemoryRepository::mapRow(const Row& row)
{
    Memory m;
    m.id = row.requireInt64(QStringLiteral("id"));
    m.name = row.requireString(QStringLiteral("name"));
    m.kind = memoryKindFromString(row.requireString(QStringLiteral("kind")))
                 .value_or(MemoryKind::Memory);
    m.persistent = row.boolean(QStringLiteral("persistent"));
    m.notes = row.string(QStringLiteral("notes"));
    return m;
}

} // namespace boh
