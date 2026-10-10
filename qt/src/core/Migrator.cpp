#include "Migrator.h"

#include <QCoreApplication>
#include <QFile>
#include <QStringList>

namespace boh {

namespace {
QString numbered(int n) { return QString::number(n); }
} // namespace

// MARK: - Errors

MigratorError::MigratorError(Kind kind, QString message)
    : std::runtime_error(message.toUtf8().constData())
    , m_kind(kind)
    , m_message(std::move(message))
{
}

// MARK: - Migrator

Migrator::Migrator(std::vector<Migration> migrations)
{
    if (migrations.empty()) {
        throw MigratorError(MigratorError::Kind::NoMigrationsFound,
                            QStringLiteral("no migrations found at the provided list"));
    }
    std::sort(migrations.begin(), migrations.end(),
              [](const Migration& a, const Migration& b) { return a.number < b.number; });
    int expected = 1;
    for (const Migration& migration : migrations) {
        if (migration.number != expected) {
            throw MigratorError(MigratorError::Kind::SequenceGap,
                                QStringLiteral("migration numbers must be contiguous: expected #%1, found #%2")
                                    .arg(numbered(expected), numbered(migration.number)));
        }
        ++expected;
    }
    m_migrations = std::move(migrations);
}

std::vector<Migration> Migrator::pending(const SQLiteDatabase& db) const
{
    const int version = db.userVersion();
    const int latest = m_migrations.empty() ? 0 : m_migrations.back().number;
    // The db knows schema states this app's migrations have never seen
    // — run nothing against it silently (fail loud at launch).
    if (version > latest) {
        throw MigratorError(MigratorError::Kind::DatabaseNewer,
                            QStringLiteral("database is at schema v%1 but this app only knows migrations "
                                           "through v%2 — the app is older than the database; "
                                           "update it, or restore a matching db.")
                                .arg(numbered(version), numbered(latest)));
    }
    std::vector<Migration> result;
    for (const Migration& migration : m_migrations) {
        if (migration.number > version)
            result.push_back(migration);
    }
    return result;
}

void Migrator::apply(SQLiteDatabase& db) const
{
    for (const Migration& migration : pending(db)) {
        // 001 drops the pre-project legacy tables; refuse if they still hold data (D8).
        if (migration.number == 1)
            legacySafetyCheck(db);
        try {
            db.executeScript(migration.sql);
        } catch (const std::exception& e) {
            // The failed file's transaction may still be open (failure between
            // BEGIN and COMMIT) — close it and restore FK enforcement so the
            // connection stays usable for the caller's error handling.
            try {
                db.executeScript(QStringLiteral("ROLLBACK;"));
            } catch (...) {
            }
            try {
                db.executeScript(QStringLiteral("PRAGMA foreign_keys = ON;"));
            } catch (...) {
            }
            throw MigratorError(MigratorError::Kind::ApplyFailed,
                                QStringLiteral("migration #%1 '%2' failed: %3")
                                    .arg(numbered(migration.number), migration.name,
                                         QString::fromUtf8(e.what())));
        }
        const int after = db.userVersion();
        if (after < migration.number) {
            throw MigratorError(MigratorError::Kind::VersionNotAdvanced,
                                QStringLiteral("migration #%1 '%2' finished but user_version is still %3")
                                    .arg(numbered(migration.number), migration.name, numbered(after)));
        }
    }
}

void Migrator::legacySafetyCheck(const SQLiteDatabase& db) const
{
    int rows = 0;
    for (const char* name : {"Books", "Memories"}) {
        const QString tableName = QString::fromUtf8(name);
        bool exists = false;
        try {
            exists = db.tableExists(tableName);
        } catch (...) {
            continue;
        }
        if (!exists)
            continue;
        try {
            rows += db.scalarInt(QStringLiteral("SELECT COUNT(*) FROM %1;").arg(tableName));
        } catch (...) {
        }
    }
    if (rows > 0) {
        throw MigratorError(MigratorError::Kind::LegacyDataPresent,
                            QStringLiteral("refusing to apply migration 001: the legacy Books/Memories tables contain "
                                           "%1 row(s). Export that data first, then apply (see docs/DATABASE.md)")
                                .arg(numbered(rows)));
    }
}

// MARK: - Loading

std::vector<Migration> Migrator::parse(const QDir& directory)
{
    const QStringList entries = directory.entryList({QStringLiteral("*.sql")}, QDir::Files);
    std::vector<Migration> migrations;
    for (const QString& fileName : entries) {
        const QString base = fileName.section(QLatin1Char('.'), 0, 0);
        const qsizetype underscore = base.indexOf(QLatin1Char('_'));
        bool numberOk = false;
        const int number = underscore > 0 ? base.left(underscore).toInt(&numberOk) : 0;
        if (underscore <= 0 || !numberOk) {
            throw MigratorError(MigratorError::Kind::BadFileName,
                                QStringLiteral("migration file names must look like NNN_description.sql — '%1'")
                                    .arg(fileName));
        }
        QFile file(directory.filePath(fileName));
        if (!file.open(QIODevice::ReadOnly)) {
            throw MigratorError(MigratorError::Kind::UnreadableMigration,
                                QStringLiteral("cannot read migration '%1': not UTF-8 text").arg(fileName));
        }
        migrations.push_back(Migration{number, base.mid(underscore + 1),
                                       QString::fromUtf8(file.readAll())});
    }
    if (migrations.empty()) {
        throw MigratorError(MigratorError::Kind::NoMigrationsFound,
                            QStringLiteral("no migrations found at %1").arg(directory.path()));
    }
    std::sort(migrations.begin(), migrations.end(),
              [](const Migration& a, const Migration& b) { return a.number < b.number; });
    return migrations;
}

std::vector<Migration> Migrator::loadFromDirectory(const QString& directory)
{
    return parse(QDir(directory));
}

std::optional<std::vector<Migration>> Migrator::loadFromMainResource(const QString& resourceDir)
{
    if (resourceDir.isEmpty())
        return std::nullopt;
    const QDir migrationsDir(resourceDir + QStringLiteral("/Migrations"));
    if (!migrationsDir.exists())
        return std::nullopt;
    std::vector<Migration> list = parse(migrationsDir);
    if (list.empty())
        return std::nullopt;
    return list;
}

std::vector<Migration> Migrator::bundled()
{
    const QDir resourceDir(QStringLiteral(":/migrations"));
    const QStringList entries = resourceDir.entryList({QStringLiteral("*.sql")}, QDir::Files);
    if (entries.empty()) {
        throw MigratorError(MigratorError::Kind::NoMigrationsFound,
                            QStringLiteral("no migrations found at the app bundle's embedded migrations"));
    }
    std::vector<Migration> migrations;
    for (const QString& fileName : entries) {
        QFile file(QStringLiteral(":/migrations/") + fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            throw MigratorError(MigratorError::Kind::UnreadableMigration,
                                QStringLiteral("cannot read migration '%1': not UTF-8 text").arg(fileName));
        }
        const QString base = fileName.section(QLatin1Char('.'), 0, 0);
        const qsizetype underscore = base.indexOf(QLatin1Char('_'));
        migrations.push_back(Migration{base.left(underscore).toInt(), base.mid(underscore + 1),
                                       QString::fromUtf8(file.readAll())});
    }
    std::sort(migrations.begin(), migrations.end(),
              [](const Migration& a, const Migration& b) { return a.number < b.number; });
    return migrations;
}

Migrator Migrator::resolve()
{
    const QString envPath = qEnvironmentVariable("BOH_MIGRATIONS");
    if (!envPath.isEmpty())
        return Migrator(loadFromDirectory(envPath));

    const QDir cwd = QDir::current();
    for (const QString& relative : {QStringLiteral("db/migrations"), QStringLiteral("../db/migrations")}) {
        const QString path = QDir::cleanPath(cwd.filePath(relative));
        if (!QDir(path).exists())
            continue;
        auto list = loadFromDirectory(path);
        if (!list.empty())
            return Migrator(std::move(list));
    }

    // Packaged app: plain Migrations/ files next to the executable — no bundle
    // machinery to fail at runtime (the 2026-10-09 launch crash was exactly
    // that failure mode on the mac side).
    if (QCoreApplication::instance()) {
        if (auto list = loadFromMainResource(QCoreApplication::applicationDirPath()))
            return Migrator(std::move(*list));
    }
    return Migrator(bundled());
}

} // namespace boh
