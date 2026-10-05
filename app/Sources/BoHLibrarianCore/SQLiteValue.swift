import Foundation

/// A value bound into a SQLite statement.
public enum SQLiteValue: Equatable {
    case int(Int64)
    case double(Double)
    case text(String)
    case blob(Data)
    case null
}

/// Swift types that can be bound as SQLite parameters. Optionals bind as NULL.
public protocol SQLiteBindable {
    var sqliteValue: SQLiteValue { get }
}

extension SQLiteValue: SQLiteBindable {
    public var sqliteValue: SQLiteValue { self }
}

extension Int: SQLiteBindable {
    public var sqliteValue: SQLiteValue { .int(Int64(self)) }
}

extension Int64: SQLiteBindable {
    public var sqliteValue: SQLiteValue { .int(self) }
}

extension Bool: SQLiteBindable {
    public var sqliteValue: SQLiteValue { .int(self ? 1 : 0) }
}

extension Double: SQLiteBindable {
    public var sqliteValue: SQLiteValue { .double(self) }
}

extension String: SQLiteBindable {
    public var sqliteValue: SQLiteValue { .text(self) }
}

extension Data: SQLiteBindable {
    public var sqliteValue: SQLiteValue { .blob(self) }
}

extension Optional: SQLiteBindable where Wrapped: SQLiteBindable {
    public var sqliteValue: SQLiteValue {
        switch self {
        case .some(let wrapped): return wrapped.sqliteValue
        case .none: return .null
        }
    }
}