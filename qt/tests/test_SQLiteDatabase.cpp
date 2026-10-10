// Port of app/Tests/BoHLibrarianCoreTests/SQLiteDatabaseTests.swift — the test
// suite is the contract for the C++ wrapper (plan Task 2). Same cases, same
// names, same assertions. Qt adaptation: `double()` accessor is `real()`
// (`double` is a keyword), Swift Data is QByteArray, nil is std::nullopt.
#include <QtTest/QtTest>

#include "SQLiteDatabase.h"
#include "SQLiteValue.h"

#include <optional>
#include <tuple>
#include <vector>

using boh::Row;
using boh::SQLiteError;
using boh::SQLiteDatabase;
using boh::SQLiteValue;

class SQLiteDatabaseTests final : public QObject {
    Q_OBJECT

private slots:
    void init() { m_db = std::make_unique<SQLiteDatabase>(QStringLiteral(":memory:")); }
    void cleanup() { m_db.reset(); }

    // MARK: Connection

    void foreignKeysAreEnabledOnEveryConnection()
    {
        QCOMPARE(m_db->scalarInt(QStringLiteral("PRAGMA foreign_keys;")), 1);
    }

    void openFailsForUnwritablePath()
    {
        try {
            SQLiteDatabase db(QStringLiteral("/definitely/not/a/real/place/Boh.db"));
            QFAIL("expected SQLiteError");
        } catch (const SQLiteError& e) {
            QVERIFY(QByteArray(e.what()).contains("cannot open database"));
        }
    }

    // MARK: Execute / query / bind

    void executeQueryRoundTripAllTypes()
    {
        m_db->execute(QStringLiteral("CREATE TABLE t (a INTEGER, b TEXT, c REAL, d BLOB, e INTEGER);"));
        m_db->execute(QStringLiteral("INSERT INTO t VALUES (?, ?, ?, ?, ?);"),
                      {SQLiteValue(42), SQLiteValue(QStringLiteral("hush")), SQLiteValue(2.5),
                       SQLiteValue(QByteArray("\x01\x02\x03", 3)), SQLiteValue(true)});

        const auto rows = m_db->query<std::tuple<std::optional<qint64>, std::optional<QString>,
                                                 std::optional<double>, std::optional<QByteArray>, bool>>(
            QStringLiteral("SELECT a, b, c, d, e FROM t;"), {},
            [](const Row& row) {
                return std::make_tuple(row.int64(QStringLiteral("a")), row.string(QStringLiteral("b")),
                                       row.real(QStringLiteral("c")), row.data(QStringLiteral("d")),
                                       row.boolean(QStringLiteral("e")));
            });
        QCOMPARE(rows.size(), 1);
        QCOMPARE(std::get<0>(rows[0]), std::optional<qint64>(42));
        QCOMPARE(std::get<1>(rows[0]), std::optional<QString>(QStringLiteral("hush")));
        QCOMPARE(std::get<2>(rows[0]), std::optional<double>(2.5));
        QCOMPARE(std::get<3>(rows[0]), std::optional<QByteArray>(QByteArray("\x01\x02\x03", 3)));
        QCOMPARE(std::get<4>(rows[0]), true);
    }

    void nullsComeBackAsNil()
    {
        m_db->execute(QStringLiteral("CREATE TABLE t (a INTEGER, b TEXT);"));
        m_db->execute(QStringLiteral("INSERT INTO t VALUES (?, ?);"),
                      {SQLiteValue(), SQLiteValue()});

        const auto rows = m_db->query<std::pair<std::optional<qint64>, std::optional<QString>>>(
            QStringLiteral("SELECT a, b FROM t;"), {},
            [](const Row& row) {
                return std::make_pair(row.int64(QStringLiteral("a")), row.string(QStringLiteral("b")));
            });
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].first.has_value(), false);
        QCOMPARE(rows[0].second.has_value(), false);
    }

    void bindParametersFilterRows()
    {
        m_db->execute(QStringLiteral("CREATE TABLE t (name TEXT);"));
        m_db->execute(QStringLiteral("INSERT INTO t VALUES (?);"), {SQLiteValue(QStringLiteral("Lantern"))});
        m_db->execute(QStringLiteral("INSERT INTO t VALUES (?);"), {SQLiteValue(QStringLiteral("Nectar"))});
        const auto rows = m_db->query<QString>(
            QStringLiteral("SELECT name FROM t WHERE name = ?;"), {SQLiteValue(QStringLiteral("Nectar"))},
            [](const Row& row) { return row.requireString(QStringLiteral("name")); });
        QCOMPARE(rows, std::vector<QString>{QStringLiteral("Nectar")});
    }

    void constraintViolationThrows()
    {
        m_db->execute(QStringLiteral("CREATE TABLE t (a TEXT NOT NULL);"));
        try {
            m_db->execute(QStringLiteral("INSERT INTO t VALUES (?);"), {SQLiteValue()});
            QFAIL("expected SQLiteError");
        } catch (const SQLiteError& e) {
            QVERIFY(QByteArray(e.what()).contains("step failed"));
        }
    }

    void requireAccessorsThrowOnMissingColumn()
    {
        m_db->execute(QStringLiteral("CREATE TABLE t (a TEXT);"));
        m_db->execute(QStringLiteral("INSERT INTO t VALUES ('x');"));
        try {
            m_db->query<qint64>(QStringLiteral("SELECT a FROM t;"), {},
                                [](const Row& row) { return row.requireInt64(QStringLiteral("typo")); });
            QFAIL("expected RowError");
        } catch (const boh::RowError& e) {
            QVERIFY(QByteArray(e.what()).contains("typo"));
        }
    }

    // MARK: Scalars, scripts, transactions, metadata

    void scalarIntReturnsZeroForEmptyResult()
    {
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE name = 'nope';")), 0);
    }

    void lastInsertRowID()
    {
        m_db->execute(QStringLiteral("CREATE TABLE t (id INTEGER PRIMARY KEY AUTOINCREMENT, a TEXT);"));
        m_db->execute(QStringLiteral("INSERT INTO t (a) VALUES (?);"), {SQLiteValue(QStringLiteral("first"))});
        QCOMPARE(m_db->lastInsertRowID(), qint64(1));
    }

    void executeScriptRunsMultipleStatements()
    {
        m_db->executeScript(QStringLiteral(
            "CREATE TABLE t (a TEXT);\n"
            "INSERT INTO t VALUES ('one');\n"
            "INSERT INTO t VALUES ('two');\n"));
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM t;")), 2);
    }

    void transactionCommits()
    {
        m_db->execute(QStringLiteral("CREATE TABLE t (a TEXT);"));
        m_db->transaction([&] {
            m_db->execute(QStringLiteral("INSERT INTO t VALUES (?);"), {SQLiteValue(QStringLiteral("kept"))});
        });
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM t;")), 1);
    }

    void transactionRollsBackOnError()
    {
        m_db->execute(QStringLiteral("CREATE TABLE t (a TEXT);"));
        struct Boom {};
        try {
            m_db->transaction([&] {
                m_db->execute(QStringLiteral("INSERT INTO t VALUES (?);"), {SQLiteValue(QStringLiteral("lost"))});
                throw Boom{};
            });
            QFAIL("expected Boom");
        } catch (const Boom&) {
            // expected
        }
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM t;")), 0);
    }

    void tableExists()
    {
        m_db->execute(QStringLiteral("CREATE TABLE real_one (a TEXT);"));
        QCOMPARE(m_db->tableExists(QStringLiteral("real_one")), true);
        QCOMPARE(m_db->tableExists(QStringLiteral("not_here")), false);
    }

    void userVersionRoundTrip()
    {
        QCOMPARE(m_db->userVersion(), 0);
        m_db->setUserVersion(7);
        QCOMPARE(m_db->userVersion(), 7);
    }

private:
    std::unique_ptr<SQLiteDatabase> m_db;
};

QTEST_MAIN(SQLiteDatabaseTests)
#include "test_SQLiteDatabase.moc"
