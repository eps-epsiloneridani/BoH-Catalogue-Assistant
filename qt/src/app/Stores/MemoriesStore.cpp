#include "MemoriesStore.h"

namespace boh {

MemoriesStore::MemoriesStore(SQLiteDatabase& db, qint64 playthroughID, QObject* parent)
    : QObject(parent)
    , m_db(db)
    , m_repo(std::make_unique<MemoryRepository>(db, playthroughID))
    , m_bookRepo(std::make_unique<BookRepository>(db, playthroughID))
{
    for (const Principle& p : PrincipleRepository(db).all())
        m_principlesByID.insert(p.id, p);
    reload();
}

std::vector<Memory> MemoriesStore::displayed() const
{
    QHash<qint64, QString> principleNames;
    for (auto it = m_principlesByID.cbegin(); it != m_principlesByID.cend(); ++it)
        principleNames.insert(it.key(), it.value().name);
    return MemoryFiltering::apply(m_memories, m_options, principleNames);
}

std::optional<Memory> MemoriesStore::selectedMemory() const
{
    if (!m_selectedMemoryID)
        return std::nullopt;
    for (const Memory& memory : displayed()) {
        if (memory.id == *m_selectedMemoryID)
            return memory;
    }
    return std::nullopt;
}

QString MemoriesStore::principleName(std::optional<qint64> id) const
{
    return id && m_principlesByID.contains(*id) ? m_principlesByID.value(*id).name : QString();
}

QString MemoriesStore::principleColor(std::optional<qint64> id) const
{
    if (const auto color = (id && m_principlesByID.contains(*id))
                               ? m_principlesByID.value(*id).color
                               : std::nullopt)
        return *color;
    return QString();
}

std::vector<BookRef> MemoriesStore::booksForLinking() const
{
    std::vector<BookRef> out;
    for (const Book& book : m_bookRepo->all())
        out.push_back(BookRef{book.id, book.title});
    return out;
}

std::vector<MemorySource> MemoriesStore::sources(qint64 memoryID) const
{
    std::vector<MemorySource> out;
    for (const MemorySource& s : m_sourcesByID.value(memoryID))
        out.push_back(s);
    return out;
}

std::vector<BookRef> MemoriesStore::yielding(qint64 memoryID) const
{
    std::vector<BookRef> out;
    for (const BookRef& ref : m_yieldingByID.value(memoryID))
        out.push_back(ref);
    return out;
}

std::vector<BookRef> MemoriesStore::allYielding(qint64 memoryID) const
{
    return m_repo->allYielding(memoryID);
}

void MemoriesStore::reload()
{
    try {
        // Earned memories only — imports carry yields of unmastered books that
        // the player can't know yet; all() stays all-inclusive for pickers.
        m_memories = m_repo->allKnown();
        m_sourcesByID.clear();
        const auto sources = m_repo->allSourcesByMemory();
        for (auto it = sources.cbegin(); it != sources.cend(); ++it)
            m_sourcesByID.insert(it.key(), std::vector<MemorySource>(it.value().cbegin(), it.value().cend()));
        m_yieldingByID.clear();
        const auto yielding = m_repo->yieldingByMemory();
        for (auto it = yielding.cbegin(); it != yielding.cend(); ++it)
            m_yieldingByID.insert(it.key(), std::vector<BookRef>(it.value().cbegin(), it.value().cend()));
    } catch (const std::exception& e) {
        m_lastError = QString::fromUtf8(e.what());
    }
    emit changed();
}

bool MemoriesStore::perform(const QString& label, const std::function<void()>& operation)
{
    try {
        operation();
        reload();
        return true;
    } catch (const std::exception& e) {
        m_lastError = QStringLiteral("%1 failed: %2").arg(label, QString::fromUtf8(e.what()));
        return false;
    }
}

void MemoriesStore::add(const MemoryDraft& draft)
{
    perform(QStringLiteral("Adding memory"), [&] {
        m_selectedMemoryID = m_repo->insert(draft).id;
    });
}

void MemoriesStore::update(const Memory& memory)
{
    perform(QStringLiteral("Saving memory"), [&] { m_repo->update(memory); });
}

void MemoriesStore::update(const Memory& original, const MemoryDraft& draft)
{
    Memory memory = original;
    memory.name = draft.name;
    memory.kind = draft.kind;
    memory.persistent = draft.persistent;
    memory.notes = draft.notes;
    memory.aspects.clear();
    for (const AspectDraft& aspect : draft.aspects) {
        memory.aspects.push_back(
            Aspect{aspect.principleID, principleName(aspect.principleID), aspect.level});
    }
    update(memory);
}

void MemoriesStore::updateNotes(const Memory& memory, const QString& notes)
{
    Memory edited = memory;
    edited.notes = notes.isEmpty() ? std::nullopt : std::optional<QString>(notes);
    update(edited);
}

void MemoriesStore::remove(qint64 id)
{
    perform(QStringLiteral("Deleting memory"), [&] {
        m_repo->remove(id);
        if (m_selectedMemoryID == id)
            m_selectedMemoryID.reset();
    });
}

void MemoriesStore::setSources(qint64 memoryID, const std::vector<MemorySource>& sources)
{
    perform(QStringLiteral("Saving sources"), [&] { m_repo->setSources(memoryID, sources); });
}

void MemoriesStore::setYieldingBooks(qint64 memoryID, const std::vector<qint64>& bookIDs)
{
    perform(QStringLiteral("Saving yielding books"),
            [&] { m_repo->setYieldingBooks(memoryID, bookIDs); });
}

} // namespace boh
