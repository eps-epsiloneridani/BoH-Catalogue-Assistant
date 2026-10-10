#include "JournalStore.h"

namespace boh {

JournalStore::JournalStore(SQLiteDatabase& db, qint64 playthroughID, QObject* parent)
    : QObject(parent)
    , m_db(db)
    , m_repo(std::make_unique<JournalRepository>(db, playthroughID))
    , m_bookRepo(std::make_unique<BookRepository>(db, playthroughID))
    , m_memoryRepo(std::make_unique<MemoryRepository>(db, playthroughID))
    , m_skillRepo(std::make_unique<SkillRepository>(db, playthroughID))
{
    reload();
}

std::vector<JournalStore::Row> JournalStore::rows() const
{
    const QString query = searchText.trimmed();
    std::vector<JournalEntry> filtered;
    if (query.isEmpty()) {
        filtered = m_entries;
    } else {
        const QString needle = query.toLower();
        for (const JournalEntry& entry : m_entries) {
            QStringList parts;
            parts << entry.entry << entry.gameDay.value_or(QString());
            if (parts.join(QLatin1Char(' ')).toLower().contains(needle))
                filtered.push_back(entry);
        }
    }
    std::vector<Row> rows;
    std::optional<QString> previousDay;
    bool hasPrevious = false;
    for (const JournalEntry& entry : filtered) {
        std::optional<QString> header;
        if (!hasPrevious || entry.gameDay != previousDay)
            header = entry.gameDay ? entry.gameDay
                                   : std::optional<QString>(QStringLiteral("No in-game day noted"));
        rows.push_back(Row{header, entry});
        previousDay = entry.gameDay;
        hasPrevious = true;
    }
    return rows;
}

void JournalStore::reload()
{
    try {
        m_entries = m_repo->recent(500);
        refreshLinkData();
    } catch (const std::exception& e) {
        m_lastError = QString::fromUtf8(e.what());
    }
    emit changed();
}

bool JournalStore::perform(const QString& label, const std::function<void()>& operation)
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

void JournalStore::quickAdd(const QString& text, std::optional<QString> gameDay)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
    perform(QStringLiteral("Adding note"),
            [&] { m_repo->insert(JournalDraft{gameDay, trimmed, {}, {}, {}}); });
}

bool JournalStore::add(const JournalDraft& draft)
{
    try {
        m_repo->insert(draft);
        reload();
        return true;
    } catch (const std::exception& e) {
        m_lastError = QStringLiteral("Adding entry failed: %1").arg(e.what());
        return false;
    }
}

void JournalStore::update(const JournalEntry& entry)
{
    perform(QStringLiteral("Saving entry"), [&] { m_repo->update(entry); });
}

void JournalStore::remove(qint64 id)
{
    perform(QStringLiteral("Deleting entry"), [&] { m_repo->remove(id); });
}

void JournalStore::refreshLinkData()
{
    m_bookTitles.clear();
    for (const Book& book : m_bookRepo->all())
        m_bookTitles.insert(book.id, book.title);
    m_memoryNames.clear();
    m_earnedMemoryIDs.clear();
    for (const Memory& memory : m_memoryRepo->all()) {
        m_memoryNames.insert(memory.id, memory.name);
        // earnedMemoryIDs filled below from allKnown()
    }
    for (const Memory& memory : m_memoryRepo->allKnown())
        m_earnedMemoryIDs.insert(memory.id);
    m_skillNames.clear();
    for (const Skill& skill : m_skillRepo->all())
        m_skillNames.insert(skill.id, skill.name);
}

QString JournalStore::bookTitle(std::optional<qint64> id) const
{
    return id && m_bookTitles.contains(*id) ? m_bookTitles.value(*id) : QString();
}

QString JournalStore::skillName(std::optional<qint64> id) const
{
    return id && m_skillNames.contains(*id) ? m_skillNames.value(*id) : QString();
}

QString JournalStore::memoryName(std::optional<qint64> id) const
{
    if (!id || !m_memoryNames.contains(*id))
        return QString();
    return m_earnedMemoryIDs.contains(*id) ? m_memoryNames.value(*id)
                                           : QStringLiteral("unrevealed memory");
}

std::vector<BookRef> JournalStore::bookPickerList() const
{
    std::vector<BookRef> out;
    for (const Book& book : m_bookRepo->all())
        out.push_back(BookRef{book.id, book.title});
    return out;
}

std::vector<Memory> JournalStore::memoryPickerList(std::optional<qint64> linkedID) const
{
    std::vector<Memory> earned = m_memoryRepo->allKnown();
    if (!linkedID)
        return earned;
    for (const Memory& memory : earned) {
        if (memory.id == *linkedID)
            return earned; // the linked memory is earned — nothing to add
    }
    const auto linked = m_memoryRepo->get(*linkedID);
    if (!linked)
        return earned;
    Memory placeholder;
    placeholder.id = *linkedID;
    placeholder.name = QStringLiteral("unrevealed memory");
    placeholder.kind = linked->kind;
    placeholder.persistent = false;
    earned.push_back(placeholder);
    return earned;
}

std::vector<Skill> JournalStore::skillPickerList() const
{
    return m_skillRepo->all();
}

} // namespace boh
