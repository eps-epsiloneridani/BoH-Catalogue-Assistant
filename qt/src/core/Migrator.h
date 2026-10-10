// Port of Migrator.swift — applies pending migrations to a database.
// Forward-only: applied migrations are recorded via PRAGMA user_version and
// never re-run (docs/DATABASE.md §Migrations). The bundled copy is the
// qrc-embedded db/migrations (:/migrations) — one source, no sync script.
#pragma once

#include "SQLiteDatabase.h"

#include <QDir>
#include <QString>
#include <QtGlobal>

#include <optional>
#include <stdexcept>
#include <vector>

namespace boh {

struct Migration {
    int number = 0;
    QString name;
    QString sql;

    bool operator==(const Migration&) const = default;
};

class MigratorError : public std::runtime_error {
public:
    enum class Kind {
        NoMigrationsFound,
        DatabaseNewer,
        BadFileName,
        UnreadableMigration,
        SequenceGap,
        LegacyDataPresent,
        ApplyFailed,
        VersionNotAdvanced,
    };

    MigratorError(Kind kind, QString message);
    Kind kind() const { return m_kind; }
    const QString& message() const { return m_message; }

private:
    Kind m_kind;
    QString m_message;
};

class Migrator {
public:
    /// Migrations must be numbered contiguously from 1.
    explicit Migrator(std::vector<Migration> migrations);

    const std::vector<Migration>& migrations() const { return m_migrations; }

    std::vector<Migration> pending(const SQLiteDatabase& db) const;
    void apply(SQLiteDatabase& db) const;

    /// Parse migration files from a directory, sorted by number.
    static std::vector<Migration> loadFromDirectory(const QString& directory);

    /// Migrations shipped as plain files in `<resourceDir>/Migrations`
    /// (nullopt when absent or empty — callers fall through to the bundled copy).
    static std::optional<std::vector<Migration>> loadFromMainResource(const QString& resourceDir);

    /// Migrations embedded from db/migrations via the qrc (:/migrations/…).
    static std::vector<Migration> bundled();

    /// Resolution order (D5, D7): `BOH_MIGRATIONS` → `./db/migrations` →
    /// `../db/migrations` → plain main-resource Migrations/ → bundled copy.
    /// A PRESENT directory must load — silently falling back to the bundled
    /// copy would run a stale schema against a knowingly-edited repo.
    static Migrator resolve();

private:
    static std::vector<Migration> parse(const QDir& directory);
    void legacySafetyCheck(const SQLiteDatabase& db) const;

    std::vector<Migration> m_migrations;
};

} // namespace boh
