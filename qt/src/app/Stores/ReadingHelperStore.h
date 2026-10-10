// Port of ReadingHelperStore.swift — book picker + the live "desk math".
#pragma once

#include "BookQuery.h"
#include "Models.h"
#include "Repositories/BookRepository.h"
#include "Repositories/LookupRepositories.h"
#include "Repositories/MemoryRepository.h"
#include "Repositories/SkillRepository.h"
#include "SQLiteDatabase.h"

#include <QHash>
#include <QObject>
#include <QSet>

#include <memory>
#include <optional>
#include <vector>

namespace boh {

class ReadingHelperStore final : public QObject {
    Q_OBJECT

public:
    struct MemoryCandidates {
        std::vector<MemoryCandidate> satisfying;
        std::vector<MemoryCandidate> nearMisses;
    };

    ReadingHelperStore(SQLiteDatabase& db, qint64 playthroughID, QObject* parent = nullptr);

    void reload();

    // MARK: Picker — the full Books-screen filtering vocabulary, unread-first.
    QString searchText;
    BookStatusFilter statusFilter = BookStatusFilter::All;
    std::optional<qint64> mysteryPrincipleID;
    BookSort sort = BookSort::Status; // unread-first — the helper's native order
    std::vector<Book> displayed() const;
    std::optional<Book> selectedBook() const; // resolved against ALL books
    std::optional<qint64> selectedBookID() const { return m_selectedBookID; }
    void setSelectedBookID(qint64 id) { m_selectedBookID = id; }
    /// Select the first displayed book on arrival, so the panel is never empty.
    void ensureSelection();

    // MARK: Lookups
    QString principleName(std::optional<qint64> id) const;
    QString principleColor(std::optional<qint64> id) const;
    QString languageName(std::optional<qint64> id) const;
    std::optional<bool> isLanguageKnown(std::optional<qint64> languageID) const;
    QString yieldedMemoryName(const Book& book) const;
    std::vector<Book> books() const { return m_books; }

    // MARK: Desk math
    MemoryCandidates memoryCandidates(const Book& book) const;
    std::vector<SkillContribution> skillContributions(const Book& book) const;

    const std::vector<Principle>& principles() const { return m_principles; }
    const std::vector<Language>& languages() const { return m_languages; }

signals:
    void changed();

private:
    std::unique_ptr<BookRepository> m_bookRepo;
    std::unique_ptr<MemoryRepository> m_memoryRepo;
    std::unique_ptr<SkillRepository> m_skillRepo;
    std::vector<Principle> m_principles;
    std::vector<Language> m_languages;
    QHash<qint64, Memory> m_memoriesByID;
    QHash<qint64, Language> m_languagesByID;
    QSet<QString> m_nativeLanguageNames;
    QSet<QString> m_knownLanguageSkills;
    std::vector<Book> m_books;
    std::optional<qint64> m_selectedBookID;
};

} // namespace boh
