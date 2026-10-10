#include "BooksStore.h"

namespace boh {

BooksStore::BooksStore(SQLiteDatabase& db, qint64 playthroughID, QObject* parent)
    : QObject(parent)
    , m_db(db)
    , m_playthroughID(playthroughID)
    , m_repo(std::make_unique<BookRepository>(db, playthroughID))
    , m_journalRepo(std::make_unique<JournalRepository>(db, playthroughID))
    , m_memoryRepo(std::make_unique<MemoryRepository>(db, playthroughID))
{
    for (const Principle& p : PrincipleRepository(db).all())
        m_principlesByID.insert(p.id, p);
    for (const Language& l : LanguageRepository(db).all()) {
        m_languagesByID.insert(l.id, l);
        if (l.native)
            m_nativeLanguageNames.insert(l.name);
    }
    reload();
}

std::vector<Book> BooksStore::displayed() const
{
    QHash<qint64, QString> principleNames;
    for (auto it = m_principlesByID.cbegin(); it != m_principlesByID.cend(); ++it)
        principleNames.insert(it.key(), it.value().name);
    QHash<qint64, QString> languageNames;
    for (auto it = m_languagesByID.cbegin(); it != m_languagesByID.cend(); ++it)
        languageNames.insert(it.key(), it.value().name);
    return BookFiltering::apply(m_books, m_options, principleNames, languageNames);
}

std::optional<Book> BooksStore::selectedBook() const
{
    if (!m_selectedBookID)
        return std::nullopt;
    const qint64 id = *m_selectedBookID;
    for (const Book& book : displayed()) {
        if (book.id == id)
            return book;
    }
    // Unfiltered fallback: a book reached via a clickable link stays on screen
    // even when the current filter/sort excludes it.
    for (const Book& book : m_books) {
        if (book.id == id)
            return book;
    }
    return std::nullopt;
}

QString BooksStore::principleName(std::optional<qint64> id) const
{
    return id && m_principlesByID.contains(*id) ? m_principlesByID.value(*id).name : QString();
}

QString BooksStore::principleColor(std::optional<qint64> id) const
{
    if (const auto color = (id && m_principlesByID.contains(*id))
                               ? m_principlesByID.value(*id).color
                               : std::nullopt)
        return *color;
    return QString();
}

QString BooksStore::languageName(std::optional<qint64> id) const
{
    return id && m_languagesByID.contains(*id) ? m_languagesByID.value(*id).name : QString();
}

QString BooksStore::memoryName(std::optional<qint64> id) const
{
    return id && m_memoriesByID.contains(*id) ? m_memoriesByID.value(*id).name : QString();
}

std::optional<bool> BooksStore::isLanguageKnown(std::optional<qint64> languageID) const
{
    if (!languageID || !m_languagesByID.contains(*languageID))
        return std::nullopt;
    const QString name = m_languagesByID.value(*languageID).name;
    return m_nativeLanguageNames.contains(name) || m_knownLanguageSkills.contains(name);
}

QStringList BooksStore::lessonSkillNames(qint64 bookID) const
{
    return m_lessonNamesByBook.value(bookID);
}

std::vector<JournalEntry> BooksStore::journalEntries(qint64 bookID) const
{
    std::vector<JournalEntry> out;
    for (const JournalEntry& entry : m_journalByBook.value(bookID))
        out.push_back(entry);
    return out;
}

std::vector<Memory> BooksStore::allMemories() const
{
    return m_memoryRepo->all();
}

std::vector<Memory> BooksStore::earnedMemories() const
{
    return m_memoryRepo->allKnown();
}

void BooksStore::reload()
{
    try {
        m_books = m_repo->all();
        m_memoriesByID.clear();
        for (const Memory& memory : m_memoryRepo->all())
            m_memoriesByID.insert(memory.id, memory);
        const auto skills = SkillRepository(m_db, m_playthroughID).all();
        m_skillNamesByID.clear();
        m_knownLanguageSkills.clear();
        for (const Skill& skill : skills) {
            m_skillNamesByID.insert(skill.id, skill.name);
            if (skill.isLanguage)
                m_knownLanguageSkills.insert(skill.name);
        }
        m_journalByBook.clear();
        const auto grouped = m_journalRepo->entriesByBook();
        for (auto it = grouped.cbegin(); it != grouped.cend(); ++it)
            m_journalByBook.insert(it.key(), std::vector<JournalEntry>(it.value().cbegin(), it.value().cend()));
        m_lessonNamesByBook.clear();
        const auto lessonAmounts = m_repo->lessonSkillAmountsByBook();
        for (auto it = lessonAmounts.cbegin(); it != lessonAmounts.cend(); ++it) {
            QStringList names;
            for (const auto& entry : it.value())
                names << (entry.amount > 1 ? QStringLiteral("%1 ×%2").arg(entry.skillName).arg(entry.amount)
                                           : entry.skillName);
            names.sort();
            m_lessonNamesByBook.insert(it.key(), names);
        }
    } catch (const std::exception& e) {
        m_lastError = QString::fromUtf8(e.what());
    }
    emit changed();
}

bool BooksStore::perform(const QString& label, const std::function<void()>& operation)
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

void BooksStore::logFormMasteredRead(qint64 bookID, const QString& title,
                                     std::optional<QString> gameDay)
{
    m_repo->recordRead(bookID);
    m_journalRepo->insert(JournalDraft{
        gameDay,
        ReadingLog::journalText([&] {
            Book b;
            b.id = bookID;
            b.title = title;
            return b;
        }(), true, {}, {}, {}, {}),
        bookID, {}, {}});
}

void BooksStore::add(const BookDraft& draft, std::optional<QString> gameDay)
{
    perform(QStringLiteral("Adding book"), [&] {
        qint64 newID = 0;
        m_db.transaction([&] {
            const Book book = m_repo->insert(draft);
            newID = book.id;
            if (BookReadTransitions::countsAsRead(nullptr, draft))
                logFormMasteredRead(book.id, book.title, gameDay);
        });
        m_selectedBookID = newID;
    });
}

void BooksStore::update(const Book& book)
{
    perform(QStringLiteral("Saving book"), [&] { m_repo->update(book); });
}

void BooksStore::update(const Book& original, const BookDraft& draft,
                        std::optional<QString> gameDay)
{
    perform(QStringLiteral("Saving book"), [&] {
        Book book = original;
        book.title = draft.title;
        book.setName = draft.setName;
        book.volume = draft.volume;
        book.bookKind = draft.bookKind;
        book.languageID = draft.languageID;
        book.mysteryPrincipleID = draft.mysteryPrincipleID;
        book.difficulty = draft.difficulty;
        book.readStatus = draft.readStatus;
        book.contamination = draft.contamination;
        book.location = draft.location;
        book.lessons = draft.lessons;
        book.yieldedMemoryID = draft.yieldedMemoryID;
        book.notes = draft.notes;
        m_repo->update(book);
        if (BookReadTransitions::countsAsRead(&original, draft))
            logFormMasteredRead(book.id, book.title, gameDay);
    });
}

void BooksStore::updateNotes(const Book& book, const QString& notes)
{
    Book edited = book;
    edited.notes = notes.isEmpty() ? std::nullopt : std::optional<QString>(notes);
    update(edited);
}

void BooksStore::setReadStatus(const Book& book, ReadStatus status, std::optional<QString> gameDay)
{
    perform(QStringLiteral("Changing read status"), [&] {
        m_repo->updateReadStatus(book.id, status);
        if (status == ReadStatus::Mastered && book.readStatus != ReadStatus::Mastered)
            logFormMasteredRead(book.id, book.title, gameDay);
    });
}

void BooksStore::remove(qint64 id)
{
    perform(QStringLiteral("Deleting book"), [&] {
        m_repo->remove(id);
        if (m_selectedBookID == id)
            m_selectedBookID.reset();
    });
}

void BooksStore::addJournalNote(qint64 bookID, const QString& text, std::optional<QString> gameDay)
{
    perform(QStringLiteral("Adding note"), [&] {
        m_journalRepo->insert(JournalDraft{gameDay, text, bookID, {}, {}});
    });
}

Memory BooksStore::createMemory(const MemoryDraft& draft)
{
    try {
        // insertOrReuse: an imported (hidden) memory of the same name+kind is the
        // same game entity — reusing it earns it via this read.
        return m_memoryRepo->insertOrReuse(draft);
    } catch (const std::exception& e) {
        m_lastError = QStringLiteral("Creating memory failed: %1").arg(e.what());
        return Memory{};
    }
}

void BooksStore::recordRead(const Book& book, bool mastering, std::optional<qint64> usedMemoryID,
                            std::optional<Memory> gainedMemory, std::optional<int> lessons,
                            std::optional<QString> gameDay, std::optional<QString> note)
{
    perform(QStringLiteral("Recording read"), [&] {
        const QString usedName = usedMemoryID ? memoryName(usedMemoryID) : QString();
        // A read that doesn't earn the memory (non-mastering on an unmastered
        // book) can't reveal its name in the journal either — mask it.
        const bool willBeMastered = mastering || book.readStatus == ReadStatus::Mastered;
        m_db.transaction([&] {
            if (mastering) {
                if (book.readStatus != ReadStatus::Mastered)
                    m_repo->updateReadStatus(book.id, ReadStatus::Mastered);
                if (lessons)
                    m_repo->setLessonsCount(book.id, lessons);
            }
            m_repo->recordRead(book.id);
            if (gainedMemory)
                m_repo->setYieldedMemory(book.id, gainedMemory->id);
            const QString gainedName = gainedMemory
                                           ? (willBeMastered ? gainedMemory->name
                                                             : QStringLiteral("unrevealed memory"))
                                           : QString();
            m_journalRepo->insert(JournalDraft{
                gameDay,
                ReadingLog::journalText(book, mastering, usedName.isEmpty() ? std::nullopt
                                                                            : std::optional<QString>(usedName),
                                        gainedName.isEmpty() ? std::nullopt
                                                             : std::optional<QString>(gainedName),
                                        lessons, note),
                book.id, gainedMemory ? std::optional<qint64>(gainedMemory->id) : std::nullopt, {}});
        });
    });
}

} // namespace boh
