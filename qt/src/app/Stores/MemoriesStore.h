// Port of MemoriesStore.swift — UI state for the Memories screen. The detail
// pane reads the live per-record caches (sourcesByID/yieldingByID), never
// snapshots of its own.
#pragma once

#include "MemoryQuery.h"
#include "Models.h"
#include "Repositories/BookRepository.h"
#include "Repositories/LookupRepositories.h"
#include "Repositories/MemoryRepository.h"
#include "SQLiteDatabase.h"

#include <QHash>
#include <QObject>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace boh {

class MemoriesStore final : public QObject {
    Q_OBJECT

public:
    MemoriesStore(SQLiteDatabase& db, qint64 playthroughID, QObject* parent = nullptr);

    void reload();
    bool perform(const QString& label, const std::function<void()>& operation);
    QString lastError() const { return m_lastError; }

    std::vector<Memory> displayed() const;
    std::optional<Memory> selectedMemory() const;

    MemoryQueryOptions& options() { return m_options; }
    std::optional<qint64> selectedMemoryID() const { return m_selectedMemoryID; }
    void setSelectedMemoryID(qint64 id) { m_selectedMemoryID = id; }
    void clearSelectedMemoryID() { m_selectedMemoryID.reset(); }

    QString principleName(std::optional<qint64> id) const;
    QString principleColor(std::optional<qint64> id) const;

    const std::vector<Memory>& memories() const { return m_memories; } // earned-only
    std::vector<BookRef> booksForLinking() const;
    std::vector<MemorySource> sources(qint64 memoryID) const;       // live cache
    std::vector<BookRef> yielding(qint64 memoryID) const;           // mastered-only, live
    std::vector<BookRef> allYielding(qint64 memoryID) const;        // edit twin, any status

    void add(const MemoryDraft& draft);
    void update(const Memory& memory);
    void update(const Memory& original, const MemoryDraft& draft);
    void updateNotes(const Memory& memory, const QString& notes);
    void remove(qint64 id);
    void setSources(qint64 memoryID, const std::vector<MemorySource>& sources);
    void setYieldingBooks(qint64 memoryID, const std::vector<qint64>& bookIDs);

signals:
    void changed();

private:
    SQLiteDatabase& m_db;
    std::unique_ptr<MemoryRepository> m_repo;
    std::unique_ptr<BookRepository> m_bookRepo;
    QHash<qint64, Principle> m_principlesByID;
    std::vector<Memory> m_memories;
    QHash<qint64, std::vector<MemorySource>> m_sourcesByID;
    QHash<qint64, std::vector<BookRef>> m_yieldingByID;
    QString m_lastError;
    MemoryQueryOptions m_options;
    std::optional<qint64> m_selectedMemoryID;
};

} // namespace boh
