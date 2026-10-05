import Foundation
import SQLite3

// Minimal, dependency-free wrapper over the system SQLite C API (D2).
// Conventions: single connection, main-thread by convention; every connection
// enables foreign keys (docs/DATABASE.md). SQL text is our own; values are bound,
// never interpolated.

// MARK: - Errors

public enum SQLiteError: Error, CustomStringConvertible {
    case openFailed(path: String, message: String)
    case prepareFailed(sql: String, message: String)
    case bindFailed(sql: String, message: String)
    case stepFailed(sql: String, message: String)
    case execFailed(sql: String, message: String)

    public var description: String {
        func head(_ sql: String) -> String { String(sql.prefix(120)) }
        switch self {
        case .openFailed(let path, let message): return "cannot open database at '\(path)': \(message)"
        case .prepareFailed(let sql, let message): return "sqlite prepare failed: \(message) — \(head(sql))"
        case .bindFailed(let sql, let message): return "sqlite bind failed: \(message) — \(head(sql))"
        case .stepFailed(let sql, let message): return "sqlite step failed: \(message) — \(head(sql))"
        case .execFailed(let sql, let message): return "sqlite exec failed: \(message) — \(head(sql))"
        }
    }
}

public enum RowError: Error, CustomStringConvertible {
    case missingColumn(String)

    public var description: String {
        switch self {
        case .missingColumn(let name): return "row has no column '\(name)' or it is NULL"
        }
    }
}

/// Tells SQLite to copy bound strings/data before returning (safe for Swift values).
private let SQLITE_TRANSIENT = unsafeBitCast(-1, to: sqlite3_destructor_type.self)

// MARK: - Database

public final class SQLiteDatabase {
    private var handle: OpaquePointer?
    private var transactionDepth = 0
    public let path: String

    public init(path: String) throws {
        self.path = path
        var raw: OpaquePointer? = nil
        let flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX
        guard sqlite3_open_v2(path, &raw, flags, nil) == SQLITE_OK, raw != nil else {
            let message = raw.map { String(cString: sqlite3_errmsg($0)) } ?? "sqlite3_open_v2 failed"
            sqlite3_close(raw)
            throw SQLiteError.openFailed(path: path, message: message)
        }
        handle = raw
        try executeScript("PRAGMA foreign_keys = ON;")
    }

    deinit { sqlite3_close_v2(handle) }

    private var lastErrorMessage: String {
        handle.map { String(cString: sqlite3_errmsg($0)) } ?? "connection closed"
    }

    private func extendedMessage(for code: Int32) -> String {
        "\(lastErrorMessage) (code \(code): \(String(cString: sqlite3_errstr(code))))"
    }

    // MARK: Statement helpers

    private func prepareStatement(_ sql: String, _ binds: [SQLiteBindable]) throws -> OpaquePointer {
        guard let handle else {
            throw SQLiteError.prepareFailed(sql: sql, message: "connection closed")
        }
        var stmt: OpaquePointer? = nil
        guard sqlite3_prepare_v2(handle, sql, -1, &stmt, nil) == SQLITE_OK, let stmt else {
            throw SQLiteError.prepareFailed(sql: sql, message: lastErrorMessage)
        }
        do { try bindValues(binds, to: stmt) } catch {
            sqlite3_finalize(stmt)
            throw error
        }
        return stmt
    }

    private func bindValues(_ binds: [SQLiteBindable], to stmt: OpaquePointer) throws {
        for (offset, bindable) in binds.enumerated() {
            let index = Int32(offset + 1)
            let rc: Int32
            switch bindable.sqliteValue {
            case .int(let value): rc = sqlite3_bind_int64(stmt, index, value)
            case .double(let value): rc = sqlite3_bind_double(stmt, index, value)
            case .text(let value): rc = sqlite3_bind_text(stmt, index, value, -1, SQLITE_TRANSIENT)
            case .blob(let value):
                rc = value.withUnsafeBytes { buffer in
                    sqlite3_bind_blob(stmt, index, buffer.baseAddress, Int32(value.count), SQLITE_TRANSIENT)
                }
            case .null: rc = sqlite3_bind_null(stmt, index)
            }
            guard rc == SQLITE_OK else {
                throw SQLiteError.bindFailed(sql: "parameter \(index)", message: lastErrorMessage)
            }
        }
    }

    // MARK: Public API

    /// Execute one DML statement (INSERT/UPDATE/DELETE) or any single statement
    /// that returns no rows, with positional `?` parameters.
    public func execute(_ sql: String, _ binds: [SQLiteBindable] = []) throws {
        let stmt = try prepareStatement(sql, binds)
        defer { sqlite3_finalize(stmt) }
        let rc = sqlite3_step(stmt)
        guard rc == SQLITE_DONE || rc == SQLITE_ROW else {
            throw SQLiteError.stepFailed(sql: sql, message: extendedMessage(for: rc))
        }
    }

    /// Run a query, mapping each row to a value.
    public func query<T>(_ sql: String, _ binds: [SQLiteBindable] = [], map: (Row) throws -> T) throws -> [T] {
        let stmt = try prepareStatement(sql, binds)
        defer { sqlite3_finalize(stmt) }
        let columns = Row.makeColumnMap(stmt)
        var results: [T] = []
        while true {
            let rc = sqlite3_step(stmt)
            if rc == SQLITE_ROW {
                results.append(try map(Row(statement: stmt, columns: columns)))
            } else if rc == SQLITE_DONE {
                break
            } else {
                throw SQLiteError.stepFailed(sql: sql, message: extendedMessage(for: rc))
            }
        }
        return results
    }

    /// First column of the first row as an integer; 0 if there are no rows.
    public func scalarInt(_ sql: String, _ binds: [SQLiteBindable] = []) throws -> Int {
        let stmt = try prepareStatement(sql, binds)
        defer { sqlite3_finalize(stmt) }
        let rc = sqlite3_step(stmt)
        guard rc == SQLITE_ROW || rc == SQLITE_DONE else {
            throw SQLiteError.stepFailed(sql: sql, message: extendedMessage(for: rc))
        }
        guard rc == SQLITE_ROW else { return 0 }
        return Int(sqlite3_column_int64(stmt, 0))
    }

    /// Execute a multi-statement script (our migration files use BEGIN…COMMIT).
    public func executeScript(_ sql: String) throws {
        guard handle != nil else {
            throw SQLiteError.execFailed(sql: sql, message: "connection closed")
        }
        var errPtr: UnsafeMutablePointer<CChar>? = nil
        let rc = sqlite3_exec(handle, sql, nil, nil, &errPtr)
        if rc != SQLITE_OK {
            let message = errPtr.map { String(cString: $0) } ?? "code \(rc)"
            sqlite3_free(errPtr)
            throw SQLiteError.execFailed(sql: sql, message: message)
        }
    }

    /// Run `body` inside a transaction, rolling back if it throws. Re-entrant:
    /// a nested `transaction` participates in the outer one (inner failures
    /// bubble up and the outermost transaction rolls everything back).
    public func transaction(_ body: () throws -> Void) throws {
        if transactionDepth > 0 {
            transactionDepth += 1
            defer { transactionDepth -= 1 }
            try body()
            return
        }
        transactionDepth = 1
        do {
            try executeScript("BEGIN IMMEDIATE;")
            try body()
            try executeScript("COMMIT;")
            transactionDepth = 0
        } catch {
            transactionDepth = 0
            try? executeScript("ROLLBACK;")
            throw error
        }
    }

    public func tableExists(_ name: String) throws -> Bool {
        try scalarInt("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?;", [name]) > 0
    }

    public var lastInsertRowID: Int64 {
        guard let handle else { return -1 }
        return sqlite3_last_insert_rowid(handle)
    }

    /// Current `PRAGMA user_version` (0 if unreadable).
    public var userVersion: Int {
        (try? scalarInt("PRAGMA user_version;")) ?? 0
    }

    public func setUserVersion(_ version: Int) throws {
        try executeScript("PRAGMA user_version = \(version);")
    }
}

// MARK: - Row

/// One row of a result set. Optional accessors return nil for missing columns
/// or NULL; `require*` variants throw so mappers fail loudly on typos.
public struct Row {
    fileprivate let statement: OpaquePointer
    fileprivate let columns: [String: Int32]

    fileprivate init(statement: OpaquePointer, columns: [String: Int32]) {
        self.statement = statement
        self.columns = columns
    }

    static func makeColumnMap(_ stmt: OpaquePointer) -> [String: Int32] {
        var map: [String: Int32] = [:]
        let count = sqlite3_column_count(stmt)
        for index in 0..<count {
            if let name = sqlite3_column_name(stmt, index) {
                map[String(cString: name)] = index
            }
        }
        return map
    }

    private func index(_ column: String) -> Int32? {
        columns[column]
    }

    private func isNull(_ index: Int32) -> Bool {
        sqlite3_column_type(statement, index) == SQLITE_NULL
    }

    public func int64(_ column: String) -> Int64? {
        guard let index = index(column), !isNull(index) else { return nil }
        return sqlite3_column_int64(statement, index)
    }

    public func int(_ column: String) -> Int? {
        int64(column).map(Int.init)
    }

    public func double(_ column: String) -> Double? {
        guard let index = index(column), !isNull(index) else { return nil }
        return sqlite3_column_double(statement, index)
    }

    public func string(_ column: String) -> String? {
        guard let index = index(column), !isNull(index),
              let text = sqlite3_column_text(statement, index) else { return nil }
        return String(cString: text)
    }

    public func data(_ column: String) -> Data? {
        guard let index = index(column), !isNull(index) else { return nil }
        guard let bytes = sqlite3_column_blob(statement, index) else { return Data() }
        let count = Int(sqlite3_column_bytes(statement, index))
        return Data(bytes: bytes, count: count)
    }

    public func bool(_ column: String) -> Bool {
        int64(column).map { $0 != 0 } ?? false
    }

    public func requireInt64(_ column: String) throws -> Int64 {
        guard let value = int64(column) else { throw RowError.missingColumn(column) }
        return value
    }

    public func requireInt(_ column: String) throws -> Int {
        guard let value = int(column) else { throw RowError.missingColumn(column) }
        return value
    }

    public func requireString(_ column: String) throws -> String {
        guard let value = string(column) else { throw RowError.missingColumn(column) }
        return value
    }
}