// Port of JournalStore.swift — the findings timeline, quick capture, and
// entity-link lookups. Unearned memory links display as placeholders.
#pragma once

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

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace boh {

class JournalStore final : public QObject {
    Q_OBJECT

public:
    /// A row with the day header that precedes it, if the in-game day changed.
    struct Row {
        std::optional<QString> header;
        JournalEntry entry;
    };

    JournalStore(SQLiteDatabase& db, qint64 playthroughID, QObject* parent = nullptr);

    void reload();
    bool perform(const QString& label, const std::function<void()>& operation);
    QString lastError() const { return m_lastError; }

    std::vector<Row> rows() const; // filtered + day-grouped

    QString searchText;
    /// Set by the Ctrl+Shift+J global command; the screen consumes + clears it.
    bool requestFocus = false;

    void quickAdd(const QString& text, std::optional<QString> gameDay);
    bool add(const JournalDraft& draft);
    void update(const JournalEntry& entry);
    void remove(qint64 id);

    const std::vector<JournalEntry>& entries() const { return m_entries; }

    // MARK: Link lookups (live caches, refilled on reload)
    QString bookTitle(std::optional<qint64> id) const;
    QString skillName(std::optional<qint64> id) const;
    /// Chip/filter display: an entry linked to an unearned memory shows a
    /// placeholder, never the unrevealed name.
    QString memoryName(std::optional<qint64> id) const;

    std::vector<BookRef> bookPickerList() const;
    /// Earned memories + a placeholder row if the entry links an unearned one.
    std::vector<Memory> memoryPickerList(std::optional<qint64> linkedID) const;
    std::vector<Skill> skillPickerList() const;

signals:
    void changed();

private:
    void refreshLinkData();

    SQLiteDatabase& m_db;
    std::unique_ptr<JournalRepository> m_repo;
    std::unique_ptr<BookRepository> m_bookRepo;
    std::unique_ptr<MemoryRepository> m_memoryRepo;
    std::unique_ptr<SkillRepository> m_skillRepo;

    std::vector<JournalEntry> m_entries;
    QHash<qint64, QString> m_bookTitles;
    QHash<qint64, QString> m_memoryNames;
    QHash<qint64, QString> m_skillNames;
    QSet<qint64> m_earnedMemoryIDs;
    QString m_lastError;
};

} // namespace boh
