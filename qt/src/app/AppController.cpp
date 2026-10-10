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
    // Stores (Task 10) are rebuilt here for the active playthrough.
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
    refreshCounts();
    emit changed();
}

} // namespace boh
