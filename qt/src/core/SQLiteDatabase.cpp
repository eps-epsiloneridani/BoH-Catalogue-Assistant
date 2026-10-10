#include "SQLiteDatabase.h"

#include <sqlite3.h>

namespace boh {

namespace {
// Tell SQLite to copy bound strings/data before returning (safe for caller-owned
// values) — same trick as the Swift wrapper.
const sqlite3_destructor_type kTransient = reinterpret_cast<sqlite3_destructor_type>(-1);

QString sqlHead(const QString& sql)
{
    return sql.left(120);
}
} // namespace

// MARK: - Errors

SQLiteError::SQLiteError(Kind kind, QString sql, QString message)
    : std::runtime_error(makeMessage(kind, sql, message).toUtf8().constData())
    , m_kind(kind)
    , m_sql(std::move(sql))
    , m_message(std::move(message))
{
}

QString SQLiteError::makeMessage(Kind kind, const QString& sql, const QString& message)
{
    switch (kind) {
    case Kind::Open:
        return QStringLiteral("cannot open database at '%1': %2").arg(sql, message);
    case Kind::Prepare:
        return QStringLiteral("sqlite prepare failed: %1 — %2").arg(message, sqlHead(sql));
    case Kind::Bind:
        return QStringLiteral("sqlite bind failed: %1 — %2").arg(message, sqlHead(sql));
    case Kind::Step:
        return QStringLiteral("sqlite step failed: %1 — %2").arg(message, sqlHead(sql));
    case Kind::Exec:
        return QStringLiteral("sqlite exec failed: %1 — %2").arg(message, sqlHead(sql));
    }
    return message;
}

RowError::RowError(const QString& column)
    : std::runtime_error(QStringLiteral("row has no column '%1' or it is NULL").arg(column).toUtf8().constData())
    , m_column(column)
{
}

// MARK: - Row

Row::Row(sqlite3_stmt* statement, const QHash<QString, int>* columns)
    : m_statement(statement)
    , m_columns(columns)
{
}

QHash<QString, int> Row::makeColumnMap(sqlite3_stmt* statement)
{
    QHash<QString, int> map;
    const int count = sqlite3_column_count(statement);
    for (int index = 0; index < count; ++index) {
        const char* name = sqlite3_column_name(statement, index);
        if (name)
            map.insert(QString::fromUtf8(name), index);
    }
    return map;
}

int Row::columnIndex(const QString& column) const
{
    const auto it = m_columns->constFind(column);
    return it == m_columns->constEnd() ? -1 : it.value();
}

bool Row::isNull(int index) const
{
    return sqlite3_column_type(m_statement, index) == SQLITE_NULL;
}

std::optional<qint64> Row::int64(const QString& column) const
{
    const int index = columnIndex(column);
    if (index < 0 || isNull(index))
        return std::nullopt;
    return sqlite3_column_int64(m_statement, index);
}

std::optional<int> Row::integer(const QString& column) const
{
    const auto value = int64(column);
    if (!value)
        return std::nullopt;
    return static_cast<int>(*value);
}

std::optional<double> Row::real(const QString& column) const
{
    const int index = columnIndex(column);
    if (index < 0 || isNull(index))
        return std::nullopt;
    return sqlite3_column_double(m_statement, index);
}

std::optional<QString> Row::string(const QString& column) const
{
    const int index = columnIndex(column);
    if (index < 0 || isNull(index))
        return std::nullopt;
    const auto* text = sqlite3_column_text(m_statement, index);
    if (!text)
        return std::nullopt;
    return QString::fromUtf8(reinterpret_cast<const char*>(text));
}

std::optional<QByteArray> Row::data(const QString& column) const
{
    const int index = columnIndex(column);
    if (index < 0 || isNull(index))
        return std::nullopt;
    const auto* bytes = sqlite3_column_blob(m_statement, index);
    const int count = sqlite3_column_bytes(m_statement, index);
    if (!bytes)
        return QByteArray();
    return QByteArray(static_cast<const char*>(bytes), count);
}

bool Row::boolean(const QString& column) const
{
    const auto value = int64(column);
    return value && *value != 0;
}

qint64 Row::requireInt64(const QString& column) const
{
    const auto value = int64(column);
    if (!value)
        throw RowError(column);
    return *value;
}

int Row::requireInteger(const QString& column) const
{
    const auto value = integer(column);
    if (!value)
        throw RowError(column);
    return *value;
}

QString Row::requireString(const QString& column) const
{
    const auto value = string(column);
    if (!value)
        throw RowError(column);
    return *value;
}

// MARK: - Database

SQLiteDatabase::SQLiteDatabase(const QString& path)
    : m_path(path)
{
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    sqlite3* raw = nullptr;
    if (sqlite3_open_v2(path.toUtf8().constData(), &raw, flags, nullptr) != SQLITE_OK || raw == nullptr) {
        const QString message = raw ? QString::fromUtf8(sqlite3_errmsg(raw))
                                    : QStringLiteral("sqlite3_open_v2 failed");
        sqlite3_close(raw);
        throw SQLiteError(SQLiteError::Kind::Open, path, message);
    }
    m_handle = raw;
    executeScript(QStringLiteral("PRAGMA foreign_keys = ON;"));
}

SQLiteDatabase::~SQLiteDatabase()
{
    sqlite3_close_v2(m_handle);
}

SQLiteDatabase::StatementGuard::~StatementGuard()
{
    sqlite3_finalize(stmt);
}

QString SQLiteDatabase::lastErrorMessage() const
{
    return m_handle ? QString::fromUtf8(sqlite3_errmsg(m_handle)) : QStringLiteral("connection closed");
}

QString SQLiteDatabase::extendedMessage(int rc) const
{
    return QStringLiteral("%1 (code %2: %3)").arg(lastErrorMessage()).arg(rc).arg(QString::fromUtf8(sqlite3_errstr(rc)));
}

sqlite3_stmt* SQLiteDatabase::prepareStatement(const QString& sql, const std::vector<SQLiteValue>& binds) const
{
    if (!m_handle)
        throw SQLiteError(SQLiteError::Kind::Prepare, sql, QStringLiteral("connection closed"));
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_handle, sql.toUtf8().constData(), -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        throw SQLiteError(SQLiteError::Kind::Prepare, sql, lastErrorMessage());
    }
    try {
        bindValues(binds, stmt);
    } catch (...) {
        sqlite3_finalize(stmt);
        throw;
    }
    return stmt;
}

void SQLiteDatabase::bindValues(const std::vector<SQLiteValue>& binds, sqlite3_stmt* stmt) const
{
    int index = 1;
    for (const SQLiteValue& value : binds) {
        int rc = SQLITE_OK;
        switch (value.type()) {
        case SQLiteValue::Type::Int:
            rc = sqlite3_bind_int64(stmt, index, value.intValue());
            break;
        case SQLiteValue::Type::Double:
            rc = sqlite3_bind_double(stmt, index, value.doubleValue());
            break;
        case SQLiteValue::Type::Text: {
            const QByteArray utf8 = value.textValue().toUtf8();
            rc = sqlite3_bind_text(stmt, index, utf8.constData(), utf8.size(), kTransient);
            break;
        }
        case SQLiteValue::Type::Blob: {
            const QByteArray& blob = value.blobValue();
            rc = sqlite3_bind_blob(stmt, index, blob.constData(), blob.size(), kTransient);
            break;
        }
        case SQLiteValue::Type::Null:
            rc = sqlite3_bind_null(stmt, index);
            break;
        }
        if (rc != SQLITE_OK) {
            throw SQLiteError(SQLiteError::Kind::Bind,
                              QStringLiteral("parameter %1").arg(index), lastErrorMessage());
        }
        ++index;
    }
}

void SQLiteDatabase::execute(const QString& sql, std::vector<SQLiteValue> binds)
{
    sqlite3_stmt* stmt = prepareStatement(sql, binds);
    StatementGuard guard{stmt};
    const int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW)
        throw SQLiteError(SQLiteError::Kind::Step, sql, extendedMessage(rc));
}

int SQLiteDatabase::scalarInt(const QString& sql, std::vector<SQLiteValue> binds) const
{
    sqlite3_stmt* stmt = prepareStatement(sql, binds);
    StatementGuard guard{stmt};
    const int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW && rc != SQLITE_DONE)
        throw SQLiteError(SQLiteError::Kind::Step, sql, extendedMessage(rc));
    if (rc != SQLITE_ROW)
        return 0;
    return static_cast<int>(sqlite3_column_int64(stmt, 0));
}

void SQLiteDatabase::executeScript(const QString& sql) const
{
    if (!m_handle)
        throw SQLiteError(SQLiteError::Kind::Exec, sql, QStringLiteral("connection closed"));
    char* errPtr = nullptr;
    const int rc = sqlite3_exec(m_handle, sql.toUtf8().constData(), nullptr, nullptr, &errPtr);
    if (rc != SQLITE_OK) {
        const QString message = errPtr ? QString::fromUtf8(errPtr) : QStringLiteral("code %1").arg(rc);
        sqlite3_free(errPtr);
        throw SQLiteError(SQLiteError::Kind::Exec, sql, message);
    }
}

bool SQLiteDatabase::tableExists(const QString& name) const
{
    return scalarInt(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?;"),
                     {SQLiteValue(name)})
           > 0;
}

qint64 SQLiteDatabase::lastInsertRowID() const
{
    if (!m_handle)
        return -1;
    return sqlite3_last_insert_rowid(m_handle);
}

int SQLiteDatabase::userVersion() const
{
    try {
        return scalarInt(QStringLiteral("PRAGMA user_version;"));
    } catch (...) {
        return 0;
    }
}

void SQLiteDatabase::setUserVersion(int version)
{
    // PRAGMA cannot take bound parameters; the value is our own int.
    executeScript(QStringLiteral("PRAGMA user_version = %1;").arg(version));
}

} // namespace boh
