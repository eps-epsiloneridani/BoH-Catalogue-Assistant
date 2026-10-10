// Plan Task 9: app-wide state — owns the single database connection, the
// playthrough lifecycle, and (from Task 10) the per-screen stores for the
// active playthrough. ONE reload choke point: reloadAll() after every write
// and on window activation — the structural fix for the empty-state/stale-
// snapshot bug family from the macOS log. The database is main-thread only.
#pragma once

#include "Models.h"
#include "Repositories/LookupRepositories.h"
#include "Repositories/PlaythroughRepository.h"
#include "SQLiteDatabase.h"
#include "Stores/BooksStore.h"
#include "Stores/JournalStore.h"
#include "Stores/MemoriesStore.h"
#include "Stores/ReadingHelperStore.h"
#include "Stores/SkillsStore.h"

#include <QObject>
#include <QString>

#include <memory>
#include <optional>
#include <vector>

namespace boh {

class AppController final : public QObject {
    Q_OBJECT

public:
    enum class Phase { Loading, Ready, Failed };
    struct Counts {
        int books = 0;
        int memories = 0;
        int skills = 0;
        int journal = 0;
        bool operator==(const Counts&) const = default;
    };

    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    void bootstrap();

    Phase phase() const { return m_phase; }
    QString failureMessage() const { return m_failureMessage; }

    SQLiteDatabase* db() const { return m_db.get(); }
    QString dbPath() const { return m_dbPath; }
    int schemaVersion() const { return m_schemaVersion; }
    const std::vector<Principle>& principles() const { return m_principles; }
    const std::vector<Language>& languages() const { return m_languages; }

    const std::vector<Playthrough>& playthroughs() const { return m_playthroughs; }
    std::optional<Playthrough> activePlaythrough() const { return m_activePlaythrough; }
    Counts counts() const { return m_counts; }

    /// Free text for "which in-game day is it" — pre-fills journal entries and
    /// the record-read dialog; kept for the whole session.
    QString currentGameDay() const { return m_currentGameDay; }
    void setCurrentGameDay(const QString& day) { m_currentGameDay = day; }

    // Screen stores (rebuilt when the active playthrough changes).
    BooksStore* booksStore() const { return m_booksStore.get(); }
    MemoriesStore* memoriesStore() const { return m_memoriesStore.get(); }
    ReadingHelperStore* helperStore() const { return m_helperStore.get(); }
    SkillsStore* skillsStore() const { return m_skillsStore.get(); }
    JournalStore* journalStore() const { return m_journalStore.get(); }

    /// Load another playthrough; remounts everything scoped to it.
    bool switchPlaythrough(qint64 id);

    /// THE choke point: call after every write and on window activation.
    void reloadAll();

signals:
    void changed();              // data reloaded — views refresh from caches
    void playthroughsChanged();  // the list or the active run changed

private:
    void remountPlaythroughState();
    void refreshCounts();

    std::unique_ptr<SQLiteDatabase> m_db;
    QString m_dbPath;
    int m_schemaVersion = 0;
    Phase m_phase = Phase::Loading;
    QString m_failureMessage;
    std::vector<Principle> m_principles;
    std::vector<Language> m_languages;
    std::vector<Playthrough> m_playthroughs;
    std::optional<Playthrough> m_activePlaythrough;
    Counts m_counts;
    QString m_currentGameDay;
    std::unique_ptr<BooksStore> m_booksStore;
    std::unique_ptr<MemoriesStore> m_memoriesStore;
    std::unique_ptr<ReadingHelperStore> m_helperStore;
    std::unique_ptr<SkillsStore> m_skillsStore;
    std::unique_ptr<JournalStore> m_journalStore;
};

} // namespace boh
