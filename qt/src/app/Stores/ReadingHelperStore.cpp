#include "ReadingHelperStore.h"

#include <algorithm>

namespace boh {

ReadingHelperStore::ReadingHelperStore(SQLiteDatabase& db, qint64 playthroughID, QObject* parent)
    : QObject(parent)
    , m_bookRepo(std::make_unique<BookRepository>(db, playthroughID))
    , m_memoryRepo(std::make_unique<MemoryRepository>(db, playthroughID))
    , m_skillRepo(std::make_unique<SkillRepository>(db, playthroughID))
{
    m_principles = PrincipleRepository(db).all();
    m_languages = LanguageRepository(db).all();
    for (const Language& l : m_languages) {
        m_languagesByID.insert(l.id, l);
        if (l.native)
            m_nativeLanguageNames.insert(l.name);
    }
    reload();
}

std::vector<Book> ReadingHelperStore::displayed() const
{
    BookQueryOptions options;
    options.searchText = searchText;
    options.statusFilter = statusFilter;
    options.mysteryPrincipleID = mysteryPrincipleID;
    options.sort = sort;
    QHash<qint64, QString> principleNames;
    for (const Principle& p : m_principles)
        principleNames.insert(p.id, p.name);
    QHash<qint64, QString> languageNames;
    for (const Language& l : m_languages)
        languageNames.insert(l.id, l.name);
    return BookFiltering::apply(m_books, options, principleNames, languageNames);
}

std::optional<Book> ReadingHelperStore::selectedBook() const
{
    if (!m_selectedBookID)
        return std::nullopt;
    for (const Book& book : m_books) {
        if (book.id == *m_selectedBookID)
            return book;
    }
    return std::nullopt;
}

void ReadingHelperStore::ensureSelection()
{
    if (!m_selectedBookID) {
        const auto shown = displayed();
        if (!shown.empty())
            m_selectedBookID = shown.front().id;
    }
}

void ReadingHelperStore::reload()
{
    m_books = m_bookRepo->all();
    m_memoriesByID.clear();
    for (const Memory& memory : m_memoryRepo->all())
        m_memoriesByID.insert(memory.id, memory);
    m_knownLanguageSkills.clear();
    for (const Skill& skill : m_skillRepo->all()) {
        if (skill.isLanguage)
            m_knownLanguageSkills.insert(skill.name);
    }
    emit changed();
}

QString ReadingHelperStore::principleName(std::optional<qint64> id) const
{
    if (!id)
        return QString();
    for (const Principle& p : m_principles) {
        if (p.id == *id)
            return p.name;
    }
    return QString();
}

QString ReadingHelperStore::principleColor(std::optional<qint64> id) const
{
    if (!id)
        return QString();
    for (const Principle& p : m_principles) {
        if (p.id == *id && p.color)
            return *p.color;
    }
    return QString();
}

QString ReadingHelperStore::languageName(std::optional<qint64> id) const
{
    return id && m_languagesByID.contains(*id) ? m_languagesByID.value(*id).name : QString();
}

std::optional<bool> ReadingHelperStore::isLanguageKnown(std::optional<qint64> languageID) const
{
    if (!languageID || !m_languagesByID.contains(*languageID))
        return std::nullopt;
    const QString name = m_languagesByID.value(*languageID).name;
    return m_nativeLanguageNames.contains(name) || m_knownLanguageSkills.contains(name);
}

QString ReadingHelperStore::yieldedMemoryName(const Book& book) const
{
    return book.yieldedMemoryID && m_memoriesByID.contains(*book.yieldedMemoryID)
               ? m_memoriesByID.value(*book.yieldedMemoryID).name
               : QString();
}

ReadingHelperStore::MemoryCandidates ReadingHelperStore::memoryCandidates(const Book& book) const
{
    MemoryCandidates out;
    if (!book.mysteryPrincipleID || !book.difficulty)
        return out;
    const auto ranked = m_memoryRepo->candidates(*book.mysteryPrincipleID, 1);
    for (const MemoryCandidate& candidate : ranked) {
        if (candidate.level >= *book.difficulty)
            out.satisfying.push_back(candidate);
    }
    auto misses = ranked;
    misses.erase(std::remove_if(misses.begin(), misses.end(),
                                [&](const MemoryCandidate& c) {
                                    return c.level >= *book.difficulty;
                                }),
                 misses.end());
    if (misses.size() > 3)
        misses.resize(3);
    out.nearMisses = std::move(misses);
    return out;
}

std::vector<SkillContribution> ReadingHelperStore::skillContributions(const Book& book) const
{
    if (!book.mysteryPrincipleID)
        return {};
    return m_skillRepo->contributions(*book.mysteryPrincipleID);
}

} // namespace boh
