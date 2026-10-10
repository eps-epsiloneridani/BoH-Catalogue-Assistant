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

} // namespace boh
