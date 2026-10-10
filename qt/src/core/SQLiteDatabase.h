// Port of SQLiteDatabase.swift — minimal, dependency-free wrapper over the
// system SQLite C API (D2). Conventions: single connection, main thread only;
// every connection enables foreign keys; SQL text is our own, values are bound
// never interpolated. Swift→C++ renames forced by keywords: `double()` →
// `real()`, `bool()` → `boolean()`, `int()` folds into `int64()`.
#pragma once

#include "SQLiteValue.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QtGlobal>

#include <sqlite3.h>

#include <functional>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace boh {

// MARK: - Errors

class SQLiteError : public std::runtime_error {
public:
    enum class Kind { Open, Prepare, Bind, Step, Exec };

    SQLiteError(Kind kind, QString sql, QString message);

    Kind kind() const { return m_kind; }
    const QString& sql() const { return m_sql; }
    const QString& message() const { return m_message; }

private:
    static QString makeMessage(Kind kind, const QString& sql, const QString& message);
    Kind m_kind;
    QString m_sql;
    QString m_message;
};

class RowError : public std::runtime_error {
public:
    explicit RowError(const QString& column);
    const QString& column() const { return m_column; }

private:
    QString m_column;
};

// MARK: - Row

/// One row of a result set. Optional accessors return nullopt for missing
/// columns or NULL; `require*` variants throw so mappers fail loudly on typos.
class Row {
public:
    std::optional<qint64> int64(const QString& column) const;
    std::optional<int> integer(const QString& column) const;
    std::optional<double> real(const QString& column) const;
    std::optional<QString> string(const QString& column) const;
    std::optional<QByteArray> data(const QString& column) const;
    bool boolean(const QString& column) const; // NULL/missing → false (Swift semantics)

    qint64 requireInt64(const QString& column) const; // throws RowError
    int requireInteger(const QString& column) const;
    QString requireString(const QString& column) const;

private:
    friend class SQLiteDatabase;
    Row(sqlite3_stmt* statement, const QHash<QString, int>* columns);

    static QHash<QString, int> makeColumnMap(sqlite3_stmt* statement);
    int columnIndex(const QString& column) const;
    bool isNull(int index) const;

    sqlite3_stmt* m_statement;
    const QHash<QString, int>* m_columns;
};

// MARK: - Database

class SQLiteDatabase {
public:
    /// Opens (creating if needed) and enables foreign keys. Throws SQLiteError.
    explicit SQLiteDatabase(const QString& path);
    ~SQLiteDatabase();

    SQLiteDatabase(const SQLiteDatabase&) = delete;
    SQLiteDatabase& operator=(const SQLiteDatabase&) = delete;
    SQLiteDatabase(SQLiteDatabase&&) = delete;
    SQLiteDatabase& operator=(SQLiteDatabase&&) = delete;

    const QString& path() const { return m_path; }

    /// Execute one DML statement (INSERT/UPDATE/DELETE) or any single statement
    /// that returns no rows, with positional `?` parameters.
    void execute(const QString& sql, std::vector<SQLiteValue> binds = {});

    /// Run a query, mapping each row to a value. `T` is the mapped row type.
    template <typename T, typename Mapper>
    std::vector<T> query(const QString& sql, std::vector<SQLiteValue> binds, Mapper&& map)
    {
        sqlite3_stmt* stmt = prepareStatement(sql, binds);
        StatementGuard guard(stmt);
        const auto columns = Row::makeColumnMap(stmt);
        std::vector<T> results;
        for (;;) {
            const int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                results.push_back(map(Row(stmt, &columns)));
            } else if (rc == SQLITE_DONE) {
                break;
            } else {
                throw SQLiteError(SQLiteError::Kind::Step, sql, extendedMessage(rc));
            }
        }
        return results;
    }

    /// First column of the first row as an integer; 0 if there are no rows.
    int scalarInt(const QString& sql, std::vector<SQLiteValue> binds = {}) const;

    /// Execute a multi-statement script (our migration files use BEGIN…COMMIT).
    void executeScript(const QString& sql) const;

    /// Run `body` inside a transaction, rolling back if it throws. Re-entrant:
    /// a nested `transaction` participates in the outer one (inner failures
    /// bubble up and the outermost transaction rolls everything back).
    template <typename Body>
    void transaction(Body&& body)
    {
        if (m_transactionDepth > 0) {
            ++m_transactionDepth;
            DepthGuard guard(m_transactionDepth);
            body();
            return;
        }
        m_transactionDepth = 1;
        try {
            executeScript(QStringLiteral("BEGIN IMMEDIATE;"));
            body();
            executeScript(QStringLiteral("COMMIT;"));
            m_transactionDepth = 0;
        } catch (...) {
            m_transactionDepth = 0;
            try {
                executeScript(QStringLiteral("ROLLBACK;"));
            } catch (...) {
                // best-effort rollback; the original error propagates
            }
            throw;
        }
    }

    bool tableExists(const QString& name) const;
    qint64 lastInsertRowID() const;

    /// Current `PRAGMA user_version` (0 if unreadable).
    int userVersion() const;
    /// PRAGMA cannot take bound parameters; the value is our own int.
    void setUserVersion(int version);

private:
    struct StatementGuard {
        explicit StatementGuard(sqlite3_stmt* s) : stmt(s) {}
        ~StatementGuard();
        StatementGuard(const StatementGuard&) = delete;
        StatementGuard& operator=(const StatementGuard&) = delete;
        sqlite3_stmt* stmt;
    };
    struct DepthGuard {
        int& depth;
        ~DepthGuard() { --depth; }
    };

    QString lastErrorMessage() const;
    QString extendedMessage(int rc) const;
    sqlite3_stmt* prepareStatement(const QString& sql, const std::vector<SQLiteValue>& binds) const;
    void bindValues(const std::vector<SQLiteValue>& binds, sqlite3_stmt* stmt) const;

    sqlite3* m_handle = nullptr;
    QString m_path;
    int m_transactionDepth = 0;
};

} // namespace boh
