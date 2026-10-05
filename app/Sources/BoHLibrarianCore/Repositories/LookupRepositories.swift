import Foundation

public final class PrincipleRepository {
    private let db: SQLiteDatabase

    public init(db: SQLiteDatabase) { self.db = db }

    public func all() throws -> [Principle] {
        try db.query("SELECT * FROM Principles ORDER BY sort_order;", map: Self.map)
    }

    public func get(_ id: Int64) throws -> Principle? {
        try db.query("SELECT * FROM Principles WHERE id = ?;", [id], map: Self.map).first
    }

    public func update(_ principle: Principle) throws {
        try db.execute(
            "UPDATE Principles SET name = ?, sort_order = ?, color = ?, notes = ? WHERE id = ?;",
            [principle.name, principle.sortOrder, principle.color, principle.notes, principle.id]
        )
    }

    static func map(_ row: Row) throws -> Principle {
        Principle(id: try row.requireInt64("id"),
                  name: try row.requireString("name"),
                  sortOrder: try row.requireInt("sort_order"),
                  color: row.string("color"),
                  notes: row.string("notes"))
    }
}

public final class LanguageRepository {
    private let db: SQLiteDatabase

    public init(db: SQLiteDatabase) { self.db = db }

    public func all() throws -> [Language] {
        try db.query("SELECT * FROM Languages ORDER BY native DESC, name;", map: Self.map)
    }

    public func get(_ id: Int64) throws -> Language? {
        try db.query("SELECT * FROM Languages WHERE id = ?;", [id], map: Self.map).first
    }

    @discardableResult
    public func insert(name: String, native: Bool, notes: String? = nil) throws -> Language {
        try db.execute(
            "INSERT INTO Languages (name, native, notes) VALUES (?, ?, ?);",
            [name, native, notes]
        )
        return Language(id: db.lastInsertRowID, name: name, native: native, notes: notes)
    }

    public func update(_ language: Language) throws {
        try db.execute(
            "UPDATE Languages SET name = ?, native = ?, notes = ? WHERE id = ?;",
            [language.name, language.native, language.notes, language.id]
        )
    }

    public func delete(_ id: Int64) throws {
        try db.execute("DELETE FROM Languages WHERE id = ?;", [id])
    }

    static func map(_ row: Row) throws -> Language {
        Language(id: try row.requireInt64("id"),
                 name: try row.requireString("name"),
                 native: row.bool("native"),
                 notes: row.string("notes"))
    }
}