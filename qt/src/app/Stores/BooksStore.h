// Port of BooksStore.swift — UI state for the Books screen: the list, query
// options, selection, and every mutation (reload-after-write).
#pragma once

#include "BookQuery.h"
#include "Models.h"
#include "Repositories/BookRepository.h"
#include "Repositories/JournalRepository.h"
#include "Repositories/LookupRepositories.h"
#include "Repositories/MemoryRepository.h"
#include "Repositories/SkillRepository.h"
#include "SQLiteDatabase.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace boh {

class BooksStore final : public QObject {
    Q_OBJECT

public:
    BooksStore(SQLiteDatabase& db, qint64 playthroughID, QObject* parent = nullptr);

    void reload();
    /// Runs one mutation, reloads on success, records the error otherwise.
    bool perform(const QString& label, const std::function<void()>& operation);
    QString lastError() const { return m_lastError; }
    void clearError() { m_lastError.clear(); }

    // MARK: Derived
    const std::vector<Book>& books() const { return m_books; }
    std::vector<Book> displayed() const;
    std::optional<Book> selectedBook() const;

    // MARK: Options + selection
    BookQueryOptions& options() { return m_options; }
    std::optional<qint64> selectedBookID() const { return m_selectedBookID; }
    void setSelectedBookID(qint64 id) { m_selectedBookID = id; }
    void clearSelectedBookID() { m_selectedBookID.reset(); }

    // MARK: Lookups for views
    QString principleName(std::optional<qint64> id) const;
    QString principleColor(std::optional<qint64> id) const;
    QString languageName(std::optional<qint64> id) const;
    QString memoryName(std::optional<qint64> id) const;
    /// nullopt = no language recorded; true/false = whether the Librarian can read it.
    std::optional<bool> isLanguageKnown(std::optional<qint64> languageID) const;
    QStringList lessonSkillNames(qint64 bookID) const;         // live cache
    std::vector<JournalEntry> journalEntries(qint64 bookID) const; // live cache
    std::vector<Memory> allMemories() const;
    std::vector<Memory> earnedMemories() const; // "Memory used" can't offer unearned

    // MARK: Mutations
    void add(const BookDraft& draft, std::optional<QString> gameDay = {});
    void update(const Book& book);
    void update(const Book& original, const BookDraft& draft, std::optional<QString> gameDay = {});
    void updateNotes(const Book& book, const QString& notes);
    void setReadStatus(const Book& book, ReadStatus status, std::optional<QString> gameDay = {});
    void remove(qint64 id);
    void addJournalNote(qint64 bookID, const QString& text, std::optional<QString> gameDay);

    // MARK: Record-a-read flow
    Memory createMemory(const MemoryDraft& draft); // insertOrReuse — may return null on error
    void recordRead(const Book& book, bool mastering, std::optional<qint64> usedMemoryID,
                    std::optional<Memory> gainedMemory, std::optional<int> lessons,
                    std::optional<QString> gameDay, std::optional<QString> note);

signals:
    void changed();

private:
    void logFormMasteredRead(qint64 bookID, const QString& title, std::optional<QString> gameDay);

    SQLiteDatabase& m_db;
    qint64 m_playthroughID;
    std::unique_ptr<BookRepository> m_repo;
    std::unique_ptr<JournalRepository> m_journalRepo;
    std::unique_ptr<MemoryRepository> m_memoryRepo;

    QHash<qint64, Principle> m_principlesByID;
    QHash<qint64, Language> m_languagesByID;
    QHash<qint64, Memory> m_memoriesByID;
    QHash<qint64, QString> m_skillNamesByID;
    QHash<qint64, std::vector<JournalEntry>> m_journalByBook;
    QHash<qint64, QStringList> m_lessonNamesByBook;
    QSet<QString> m_nativeLanguageNames;
    QSet<QString> m_knownLanguageSkills;

    std::vector<Book> m_books;
    QString m_lastError;
    BookQueryOptions m_options;
    std::optional<qint64> m_selectedBookID;
};

} // namespace boh
