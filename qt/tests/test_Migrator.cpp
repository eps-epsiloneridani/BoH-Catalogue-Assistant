// Port of app/Tests/BoHLibrarianCoreTests/MigratorTests.swift (plan Task 3).
// Adaptation: the 006 test's repository calls become direct SQL inserts (the
// repositories arrive in Task 6); migration-behavior assertions are identical.
// Bundled copy = the qrc-embedded db/migrations (:/migrations), not SPM resources.
#include <QtTest/QtTest>

#include "Migrator.h"
#include "SQLiteDatabase.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <filesystem>
#include <vector>

using boh::Migration;
using boh::Migrator;
using boh::MigratorError;
using boh::Row;
using boh::SQLiteDatabase;
using boh::SQLiteValue;

namespace {
QString repoMigrationsDir()
{
    // this file: <repo>/qt/tests/test_Migrator.cpp → <repo>/db/migrations
    const std::filesystem::path here(__FILE__);
    return QString::fromStdString((here.parent_path() / ".." / ".." / "db" / "migrations").string());
}

/// Restores the process cwd on scope exit (Swift test used defer).
class ScopedCwd {
public:
    explicit ScopedCwd(const QString& newDir)
        : m_old(QDir::currentPath())
    {
        QDir::setCurrent(newDir);
    }
    ~ScopedCwd() { QDir::setCurrent(m_old); }
    ScopedCwd(const ScopedCwd&) = delete;
    ScopedCwd& operator=(const ScopedCwd&) = delete;

private:
    QString m_old;
};
} // namespace

class MigratorTests final : public QObject {
    Q_OBJECT

private slots:
    void init() { m_db = std::make_unique<SQLiteDatabase>(QStringLiteral(":memory:")); }
    void cleanup() { m_db.reset(); }

    // MARK: Fresh application

    void freshApplyReachesVersion9AndSeeds()
    {
        Migrator migrator(Migrator::bundled());
        QCOMPARE((int) migrator.pending(*m_db).size(), 9);
        migrator.apply(*m_db);

        QCOMPARE(m_db->userVersion(), 9);
        for (const char* table : {"Principles", "Languages", "Memories", "MemoryAspects", "MemorySources",
                                  "Books", "BookLessons", "Skills", "Journal", "Playthroughs", "Meta"}) {
            QVERIFY2(m_db->tableExists(QString::fromUtf8(table)), table);
        }
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Principles;")), 13);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Languages;")), 15);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Playthroughs;")), 1);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Meta;")), 1);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT difficulty FROM Books LIMIT 1;")), 0);
    }

    void applyIsIdempotent()
    {
        Migrator migrator(Migrator::bundled());
        migrator.apply(*m_db);
        migrator.apply(*m_db);
        QVERIFY(migrator.pending(*m_db).empty());
        QCOMPARE(m_db->userVersion(), 9);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Principles;")), 13);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Playthroughs;")), 1);
    }

    // MARK: Validation

    void emptyMigrationListThrows()
    {
        try {
            Migrator migrator({});
            QFAIL("expected MigratorError");
        } catch (const MigratorError& e) {
            QVERIFY(QByteArray(e.what()).contains("no migrations"));
        }
    }

    void sequenceGapThrows()
    {
        const Migration one{1, QStringLiteral("one"), QStringLiteral("PRAGMA user_version = 1;")};
        const Migration three{3, QStringLiteral("three"), QStringLiteral("PRAGMA user_version = 3;")};
        try {
            Migrator migrator({three, one});
            QFAIL("expected MigratorError");
        } catch (const MigratorError& e) {
            QVERIFY(QByteArray(e.what()).contains("expected #2"));
        }
    }

    void badSQLSurfacesAsApplyFailure()
    {
        Migrator migrator({Migration{1, QStringLiteral("broken"), QStringLiteral("THIS IS NOT SQL;")}});
        try {
            migrator.apply(*m_db);
            QFAIL("expected MigratorError");
        } catch (const MigratorError& e) {
            QVERIFY(QByteArray(e.what()).contains("migration #1 'broken' failed"));
        }
        QCOMPARE(m_db->userVersion(), 0);
    }

    // MARK: Legacy guard (docs/DECISIONS.md D8)

    void legacyDataRefusesToApply()
    {
        m_db->executeScript(QStringLiteral(
            "CREATE TABLE Books (Title TEXT);\n"
            "CREATE TABLE Memories (Name TEXT);\n"
            "INSERT INTO Books VALUES ('precious find');\n"));
        Migrator migrator(Migrator::bundled());
        try {
            migrator.apply(*m_db);
            QFAIL("expected MigratorError");
        } catch (const MigratorError& e) {
            QVERIFY(QByteArray(e.what()).contains("legacy"));
        }
        QCOMPARE(m_db->userVersion(), 0);
        QVERIFY(m_db->tableExists(QStringLiteral("Books")));
    }

    void emptyLegacyTablesAreSafeToDrop()
    {
        m_db->executeScript(QStringLiteral(
            "CREATE TABLE Books (Title TEXT); CREATE TABLE Memories (Name TEXT);"));
        Migrator migrator(Migrator::bundled());
        migrator.apply(*m_db);
        QCOMPARE(m_db->userVersion(), 9);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Books;")), 0);
    }

    // MARK: 006 — per-playthrough unique names

    void p006RebuildPreservesRowsAndScopesUniqueness()
    {
        const auto all = Migrator::bundled();
        Migrator(std::vector<Migration>(all.begin(), all.begin() + 5)).apply(*m_db);
        const qint64 playthrough = m_db->scalarInt(
            QStringLiteral("SELECT CAST(value AS INTEGER) FROM Meta WHERE key = 'active_playthrough';"));

        m_db->execute(QStringLiteral(
                          "INSERT INTO Skills (name, is_language, level, wisdom, element, playthrough_id)"
                          " VALUES ('Furs & Feathers', 0, 3, 'horomachistry', 'Trist', ?);"),
                      {SQLiteValue(playthrough)});
        m_db->execute(QStringLiteral(
                          "INSERT INTO Memories (name, kind, persistent, playthrough_id)"
                          " VALUES ('Memory: Impulse', 'memory', 0, ?);"),
                      {SQLiteValue(playthrough)});
        const qint64 skillId = m_db->lastInsertRowID();
        const qint64 memoryId = m_db->lastInsertRowID();

        // The app's real path: full list, pending() picks up just 006 on the
        // now data-bearing v5 db.
        Migrator(all).apply(*m_db);
        QCOMPARE(m_db->userVersion(), 9);

        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT id FROM Skills WHERE name = 'Furs & Feathers';")), (int) skillId);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT id FROM Memories WHERE name = 'Memory: Impulse';")), (int) memoryId);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Skills WHERE wisdom = 'horomachistry';")), 1);

        m_db->execute(QStringLiteral("INSERT INTO Playthroughs (name) VALUES ('Second');"));
        const qint64 second = m_db->lastInsertRowID();
        m_db->execute(QStringLiteral(
                          "INSERT INTO Skills (name, is_language, playthrough_id) VALUES ('Furs & Feathers', 0, ?);"),
                      {SQLiteValue(second)});
        m_db->execute(QStringLiteral(
                          "INSERT INTO Memories (name, kind, playthrough_id) VALUES ('Memory: Impulse', 'memory', ?);"),
                      {SQLiteValue(second)});
        try {
            m_db->execute(QStringLiteral(
                              "INSERT INTO Skills (name, playthrough_id) VALUES ('Furs & Feathers', ?);"),
                          {SQLiteValue(playthrough)});
            QFAIL("expected UNIQUE violation");
        } catch (const boh::SQLiteError&) {
        }
        try {
            m_db->execute(QStringLiteral(
                              "INSERT INTO Memories (name, kind, playthrough_id) VALUES ('Memory: Impulse', 'memory', ?);"),
                          {SQLiteValue(playthrough)});
            QFAIL("expected UNIQUE violation");
        } catch (const boh::SQLiteError&) {
        }
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM pragma_foreign_key_check;")), 0);
    }

    // MARK: 007 — imported book kinds repaired

    void p007RepairsImportedKindStampsOnly()
    {
        const auto all = Migrator::bundled();
        Migrator(std::vector<Migration>(all.begin(), all.begin() + 6)).apply(*m_db);
        const qint64 playthrough = m_db->scalarInt(
            QStringLiteral("SELECT CAST(value AS INTEGER) FROM Meta WHERE key = 'active_playthrough';"));

        const auto seed = [&](const QString& title, const QString& kind, const QString& stamp) {
            m_db->execute(QStringLiteral(
                              "INSERT INTO Books (title, book_kind, read_status, created_at, updated_at, playthrough_id)"
                              " VALUES (?, ?, 'catalogued', ?, ?, ?);"),
                          {SQLiteValue(title), SQLiteValue(kind), SQLiteValue(stamp), SQLiteValue(stamp),
                           SQLiteValue(playthrough)});
        };
        seed(QStringLiteral("Imported Mis-stamped"), QStringLiteral("record"), QStringLiteral("2026-10-06 12:58:45"));
        seed(QStringLiteral("Imported Scroll"), QStringLiteral("scroll"), QStringLiteral("2026-10-06 12:58:45"));
        seed(QStringLiteral("Manual Record"), QStringLiteral("record"), QStringLiteral("2026-10-06 14:24:39"));
        seed(QStringLiteral("Manual Book"), QStringLiteral("book"), QStringLiteral("2026-10-06 14:29:11"));
        QVERIFY(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Books WHERE book_kind = 'record';")) == 2);

        Migrator(all).apply(*m_db);
        QCOMPARE(m_db->userVersion(), 9);

        QCOMPARE(kindOf(QStringLiteral("Imported Mis-stamped")), QStringLiteral("book"));
        QCOMPARE(kindOf(QStringLiteral("Imported Scroll")), QStringLiteral("scroll"));
        QCOMPARE(kindOf(QStringLiteral("Manual Record")), QStringLiteral("record"));
        QCOMPARE(kindOf(QStringLiteral("Manual Book")), QStringLiteral("book"));
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM pragma_foreign_key_check;")), 0);
    }

    // Migrator robustness (remediation)

    void failedMigrationLeavesConnectionUsable()
    {
        const std::vector<Migration> migrations = {
            Migration{1, QStringLiteral("one"),
                      QStringLiteral("CREATE TABLE a (x); PRAGMA user_version = 1;")},
            Migration{2, QStringLiteral("bad"),
                      QStringLiteral("BEGIN;\nCREATE TABLE half_written (x);\n"
                                     "PRAGMA user_version = 2;\nTHIS STATEMENT IS NOT SQL;\n")},
        };
        Migrator migrator(migrations);
        try {
            migrator.apply(*m_db);
            QFAIL("expected MigratorError");
        } catch (const MigratorError& e) {
            QVERIFY(QByteArray(e.what()).contains("migration #2 'bad' failed"));
        }
        QCOMPARE(m_db->userVersion(), 1);
        try {
            m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM half_written;"));
            QFAIL("expected rolled-back table to be missing");
        } catch (const boh::SQLiteError&) {
        }
        QVERIFY(m_db->tableExists(QStringLiteral("a")));
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT 1;")), 1);
    }

    void resolveFailsLoudWhenRepoMigrationsAreBroken()
    {
        QTemporaryDir fake;
        QVERIFY(fake.isValid());
        const QString broken = fake.path() + QStringLiteral("/db/migrations");
        QVERIFY(QDir().mkpath(broken));
        QFile bad(broken + QStringLiteral("/NNN_bad.sql"));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("placeholder");
        bad.close();

        const ScopedCwd cwd(fake.path());
        try {
            Migrator::resolve();
            QFAIL("expected MigratorError");
        } catch (const MigratorError& e) {
            QCOMPARE(e.kind(), MigratorError::Kind::BadFileName);
        }
    }

    void newerDatabaseIsRefused()
    {
        m_db->executeScript(QStringLiteral("PRAGMA user_version = 99;"));
        m_db->executeScript(QStringLiteral("CREATE TABLE anything (x);"));
        Migrator migrator(Migrator::bundled());
        try {
            migrator.pending(*m_db);
            QFAIL("expected MigratorError");
        } catch (const MigratorError& e) {
            QVERIFY(QByteArray(e.what()).contains("older than the database"));
        }
        QVERIFY(m_db->tableExists(QStringLiteral("anything")));
    }

    void p008HumanizesAnd009RemovesTransitRows()
    {
        const auto all = Migrator::bundled();
        Migrator(std::vector<Migration>(all.begin(), all.begin() + 7)).apply(*m_db);
        const qint64 playthrough = m_db->scalarInt(
            QStringLiteral("SELECT CAST(value AS INTEGER) FROM Meta WHERE key = 'active_playthrough';"));
        const auto seed = [&](const QString& title, const QString& location, const QString& stamp) {
            m_db->execute(QStringLiteral(
                              "INSERT INTO Books (title, location, read_status, created_at, updated_at, playthrough_id)"
                              " VALUES (?, ?, 'catalogued', ?, ?, ?);"),
                          {SQLiteValue(title), SQLiteValue(location), SQLiteValue(stamp), SQLiteValue(stamp),
                           SQLiteValue(playthrough)});
        };
        seed(QStringLiteral("Won Book"), QStringLiteral("purchases.europe"), QStringLiteral("2026-10-07 12:00:00"));
        seed(QStringLiteral("Carried Book"), QStringLiteral("portage3"), QStringLiteral("2026-10-07 12:00:00"));
        seed(QStringLiteral("Old Typed"), QStringLiteral("purchases.europe (my note)"), QStringLiteral("2026-10-05 12:00:00"));

        Migrator(all).apply(*m_db);
        QCOMPARE(m_db->userVersion(), 9);
        QVERIFY(locationOf(QStringLiteral("Won Book")).isNull());
        QCOMPARE(locationOf(QStringLiteral("Carried Book")), QStringLiteral("in portage (player inventory)"));
        QCOMPARE(locationOf(QStringLiteral("Old Typed")), QStringLiteral("purchases.europe (my note)"));
    }

    // Packaged-app migrations: plain Migrations/ files next to the main
    // executable are picked up (the packaged app never touches bundle machinery).

    void mainResourceMigrationsAreLoaded()
    {
        QTemporaryDir fake;
        QVERIFY(fake.isValid());
        const QString resourcesDir = fake.path() + QStringLiteral("/Resources");
        QVERIFY(QDir().mkpath(resourcesDir + QStringLiteral("/Migrations")));
        QFile file(resourcesDir + QStringLiteral("/Migrations/042_plain_files.sql"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("PRAGMA user_version = 42;");
        file.close();

        const auto list = Migrator::loadFromMainResource(resourcesDir);
        QVERIFY(list.has_value());
        QCOMPARE((int) list->size(), 1);
        QVERIFY(!Migrator::loadFromMainResource(QString()).has_value());

        QTemporaryDir empty;
        QVERIFY(empty.isValid());
        QVERIFY(!Migrator::loadFromMainResource(empty.path()).has_value());
    }

    // MARK: Bundled copy stays in sync with the repo

    void bundledMigrationsMatchRepoDirectory()
    {
        const QString repo = repoMigrationsDir();
        if (!QDir(repo).exists())
            QSKIP("repo db/migrations/ not present (detached resources-only run)");
        const Migrator repoMigrator(Migrator::loadFromDirectory(repo));
        const Migrator bundledMigrator(Migrator::bundled());
        QVERIFY2(repoMigrator.migrations() == bundledMigrator.migrations(),
                 "bundled migrations drifted from db/migrations/ — the qrc must list the real files");
    }

    void resolveUsesEnvVariableWhenSet()
    {
        qputenv("BOH_MIGRATIONS", repoMigrationsDir().toUtf8());
        struct Restore {
            ~Restore() { qunsetenv("BOH_MIGRATIONS"); }
        } restore;

        const Migrator migrator = Migrator::resolve();
        QCOMPARE((int) migrator.migrations().size(), 9);
    }

private:
    QString kindOf(const QString& title)
    {
        const auto rows = m_db->query<QString>(QStringLiteral("SELECT book_kind FROM Books WHERE title = ?;"),
                                               {SQLiteValue(title)},
                                               [](const boh::Row& row) { return row.requireString(QStringLiteral("book_kind")); });
        return rows.empty() ? QString() : rows[0];
    }

    QString locationOf(const QString& title)
    {
        const auto rows = m_db->query<std::optional<QString>>(
            QStringLiteral("SELECT location FROM Books WHERE title = ?;"), {SQLiteValue(title)},
            [](const boh::Row& row) { return row.string(QStringLiteral("location")); });
        return rows.empty() || !rows[0].has_value() ? QString() : *rows[0];
    }

    std::unique_ptr<SQLiteDatabase> m_db;
};

QTEST_MAIN(MigratorTests)
#include "test_Migrator.moc"
