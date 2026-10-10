#include "AppController.h"

#include "DatabaseLocation.h"
#include "Migrator.h"
#include "Repositories/BookRepository.h"
#include "Repositories/JournalRepository.h"
#include "Repositories/MemoryRepository.h"
#include "Repositories/SkillRepository.h"

#include <QFile>

namespace boh {

AppController::AppController(QObject* parent)
    : QObject(parent)
{
}

AppController::~AppController() = default;

void AppController::bootstrap()
{
    try {
        m_dbPath = DatabaseLocation::resolvePath();
        m_db = std::make_unique<SQLiteDatabase>(m_dbPath);
        // D11: the db file is user-private (created by sqlite with default perms).
        QFile::setPermissions(m_dbPath, QFile::ReadOwner | QFile::WriteOwner);
        Migrator(boh::Migrator::resolve()).apply(*m_db);
        m_schemaVersion = m_db->userVersion();
        m_principles = PrincipleRepository(*m_db).all();
        m_languages = LanguageRepository(*m_db).all();

        PlaythroughRepository repo(*m_db);
        auto active = repo.active();
        if (!active) {
            // Belt and braces: a db with no playthroughs.
            active = repo.insert(QStringLiteral("First playthrough"));
        }
        m_activePlaythrough = active;
        m_playthroughs = repo.all();
        remountPlaythroughState();
        m_phase = Phase::Ready;
    } catch (const std::exception& e) {
        m_db.reset();
        m_phase = Phase::Failed;
        m_failureMessage = QString::fromUtf8(e.what());
    }
}

void AppController::remountPlaythroughState()
{
    // Stores are rebuilt for the active playthrough; old data stays in the db —
    // switching back is one click.
    if (m_db && m_activePlaythrough) {
        const qint64 pid = m_activePlaythrough->id;
        m_booksStore = std::make_unique<BooksStore>(*m_db, pid);
        m_memoriesStore = std::make_unique<MemoriesStore>(*m_db, pid);
        m_helperStore = std::make_unique<ReadingHelperStore>(*m_db, pid);
        m_skillsStore = std::make_unique<SkillsStore>(*m_db, pid);
        m_journalStore = std::make_unique<JournalStore>(*m_db, pid);
    } else {
        m_booksStore.reset();
        m_memoriesStore.reset();
        m_helperStore.reset();
        m_skillsStore.reset();
        m_journalStore.reset();
    }
    refreshCounts();
    emit playthroughsChanged();
    emit changed();
}

void AppController::refreshCounts()
{
    if (!m_db || !m_activePlaythrough) {
        m_counts = Counts{};
        return;
    }
    const SQLiteValue pid(m_activePlaythrough->id);
    m_counts.books = m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Books WHERE playthrough_id = ?;"), {pid});
    m_counts.memories = m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Memories WHERE playthrough_id = ?;"), {pid});
    m_counts.skills = m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Skills WHERE playthrough_id = ?;"), {pid});
    m_counts.journal = m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Journal WHERE playthrough_id = ?;"), {pid});
}

bool AppController::switchPlaythrough(qint64 id)
{
    if (!m_db || !m_activePlaythrough || id == m_activePlaythrough->id)
        return false;
    PlaythroughRepository repo(*m_db);
    try {
        repo.setActiveID(id);
        m_activePlaythrough = repo.get(id);
        m_playthroughs = repo.all();
        remountPlaythroughState();
        return true;
    } catch (const std::exception& e) {
        m_failureMessage = QStringLiteral("Switching playthrough failed: %1").arg(e.what());
        return false;
    }
}

void AppController::reloadAll()
{
    if (m_phase != Phase::Ready)
        return;
    // The choke point: every store refills ALL of its caches (the structural
    // fix for the empty-state/stale-snapshot bug family), then views refresh.
    if (m_booksStore)
        m_booksStore->reload();
    if (m_memoriesStore)
        m_memoriesStore->reload();
    if (m_helperStore)
        m_helperStore->reload();
    if (m_skillsStore)
        m_skillsStore->reload();
    if (m_journalStore)
        m_journalStore->reload();
    refreshCounts();
    emit changed();
}


bool AppController::createPlaythrough(const QString& name, const QString& notes)
{
    if (!m_db)
        return false;
    try {
        PlaythroughRepository repo(*m_db);
        const QString trimmed = name.trimmed();
        const QString finalName =
            trimmed.isEmpty() ? QStringLiteral("Playthrough %1").arg(m_playthroughs.size() + 1)
                              : trimmed;
        const QString trimmedNotes = notes.trimmed();
        const Playthrough created =
            repo.insert(finalName, trimmedNotes.isEmpty() ? std::nullopt
                                                          : std::optional<QString>(trimmedNotes));
        repo.setActiveID(created.id);
        m_activePlaythrough = created;
        m_playthroughs = repo.all();
        remountPlaythroughState();
        return true;
    } catch (const std::exception& e) {
        m_lastError = QStringLiteral("Creating playthrough failed: %1").arg(e.what());
        return false;
    }
}

bool AppController::renamePlaythrough(const Playthrough& playthrough, const QString& name)
{
    if (!m_db)
        return false;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return false;
    try {
        PlaythroughRepository repo(*m_db);
        Playthrough edited = playthrough;
        edited.name = trimmed;
        repo.update(edited);
        if (m_activePlaythrough && playthrough.id == m_activePlaythrough->id)
            m_activePlaythrough = repo.get(playthrough.id);
        m_playthroughs = repo.all();
        emit playthroughsChanged();
        emit changed();
        return true;
    } catch (const std::exception& e) {
        m_lastError = QStringLiteral("Renaming playthrough failed: %1").arg(e.what());
        return false;
    }
}

bool AppController::deletePlaythrough(const Playthrough& playthrough)
{
    if (!m_db || !m_activePlaythrough)
        return false;
    if (playthrough.id == m_activePlaythrough->id)
        return false; // the UI gates this; double-checked here
    if (m_playthroughs.size() <= 1)
        return false; // never the last one
    try {
        PlaythroughRepository repo(*m_db);
        repo.remove(playthrough.id);
        m_playthroughs = repo.all();
        emit playthroughsChanged();
        emit changed();
        return true;
    } catch (const std::exception& e) {
        m_lastError = QStringLiteral("Deleting playthrough failed: %1").arg(e.what());
        return false;
    }
}

} // namespace boh
