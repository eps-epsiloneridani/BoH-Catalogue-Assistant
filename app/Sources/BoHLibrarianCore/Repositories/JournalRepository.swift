import Foundation

public final class JournalRepository {
    private let db: SQLiteDatabase

    public init(db: SQLiteDatabase) { self.db = db }

    /// Newest first.
    public func recent(limit: Int = 100) throws -> [JournalEntry] {
        try db.query(
            "SELECT * FROM Journal ORDER BY logged_at DESC, id DESC LIMIT ?;",
            [limit],
            map: Self.map
        )
    }

    public func get(_ id: Int64) throws -> JournalEntry? {
        try db.query("SELECT * FROM Journal WHERE id = ?;", [id], map: Self.map).first
    }

    /// Entries linked to a given entity, newest first.
    public func entries(bookID: Int64? = nil, memoryID: Int64? = nil,
                        skillID: Int64? = nil, limit: Int = 100) throws -> [JournalEntry] {
        var clauses: [String] = []
        var binds: [SQLiteBindable] = []
        if let bookID { clauses.append("book_id = ?"); binds.append(bookID) }
        if let memoryID { clauses.append("memory_id = ?"); binds.append(memoryID) }
        if let skillID { clauses.append("skill_id = ?"); binds.append(skillID) }
        let whereClause = clauses.isEmpty ? "" : "WHERE " + clauses.joined(separator: " AND ")
        return try db.query(
            "SELECT * FROM Journal \(whereClause) ORDER BY logged_at DESC, id DESC LIMIT ?;",
            binds + [limit],
            map: Self.map
        )
    }

    @discardableResult
    public func insert(_ draft: JournalDraft) throws -> JournalEntry {
        try db.execute(
            """
            INSERT INTO Journal (game_day, entry, book_id, memory_id, skill_id)
            VALUES (?, ?, ?, ?, ?);
            """,
            [draft.gameDay, draft.entry, draft.bookID, draft.memoryID, draft.skillID]
        )
        return try get(db.lastInsertRowID)!
    }

    public func update(_ entry: JournalEntry) throws {
        try db.execute(
            """
            UPDATE Journal
            SET game_day = ?, entry = ?, book_id = ?, memory_id = ?, skill_id = ?
            WHERE id = ?;
            """,
            [entry.gameDay, entry.entry, entry.bookID, entry.memoryID, entry.skillID, entry.id]
        )
    }

    public func delete(_ id: Int64) throws {
        try db.execute("DELETE FROM Journal WHERE id = ?;", [id])
    }

    static func map(_ row: Row) throws -> JournalEntry {
        JournalEntry(id: try row.requireInt64("id"),
                     loggedAt: try row.requireString("logged_at"),
                     gameDay: row.string("game_day"),
                     entry: try row.requireString("entry"),
                     bookID: row.int64("book_id"),
                     memoryID: row.int64("memory_id"),
                     skillID: row.int64("skill_id"))
    }
}