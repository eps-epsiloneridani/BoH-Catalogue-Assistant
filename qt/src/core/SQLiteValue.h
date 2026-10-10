// Port of SQLiteValue.swift — a value bound into a SQLite statement.
// Construction mirrors the Swift SQLiteBindable conformances: int/bool/double/
// QString/QByteArray, and a default (or explicit) null. Implicit constructors
// keep call sites shaped like the Swift bind arrays.
#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace boh {

class SQLiteValue {
public:
    enum class Type { Int, Double, Text, Blob, Null };

    SQLiteValue() = default;                              // Null
    SQLiteValue(int value) : m_type(Type::Int), m_int(value) {}
    SQLiteValue(qint64 value) : m_type(Type::Int), m_int(value) {}
    SQLiteValue(bool value) : m_type(Type::Int), m_int(value ? 1 : 0) {}
    /// Without this, string literals would bind via the pointer→bool conversion
    /// (every literal silently becoming 1) — bool stays for real bools.
    SQLiteValue(const char* value) : m_type(Type::Text), m_text(QString::fromUtf8(value)) {}
    SQLiteValue(double value) : m_type(Type::Double), m_double(value) {}
    SQLiteValue(const QString& value) : m_type(Type::Text), m_text(value) {}
    SQLiteValue(const QByteArray& value) : m_type(Type::Blob), m_blob(value) {}

    Type type() const { return m_type; }
    bool isNull() const { return m_type == Type::Null; }
    qint64 intValue() const { return m_int; }
    double doubleValue() const { return m_double; }
    const QString& textValue() const { return m_text; }
    const QByteArray& blobValue() const { return m_blob; }

private:
    Type m_type = Type::Null;
    qint64 m_int = 0;
    double m_double = 0.0;
    QString m_text;
    QByteArray m_blob;
};

/// Optionals bind as NULL when empty (Swift Optional: SQLiteBindable).
inline SQLiteValue sv(std::optional<QString> value)
{
    return value ? SQLiteValue(*value) : SQLiteValue();
}
inline SQLiteValue sv(std::optional<qint64> value)
{
    return value ? SQLiteValue(*value) : SQLiteValue();
}
inline SQLiteValue sv(std::optional<int> value)
{
    return value ? SQLiteValue(*value) : SQLiteValue();
}

} // namespace boh
