import Foundation

/// Scoped to one playthrough (docs/DATABASE.md §Playthroughs).
public final class JournalRepository {
    private let db: SQLiteDatabase
    private let playthroughID: Int64

    public init(db: SQLiteDatabase, playthroughID: Int64) {
        self.db = db
        self.playthroughID = playthroughID
    }

    /// Newest first.
    public func recent(limit: Int = 100) throws -> [JournalEntry] {
        try db.query(
            "SELECT * FROM Journal WHERE playthrough_id = ? ORDER BY logged_at DESC, id DESC LIMIT ?;",
            [playthroughID, limit],
            map: Self.map
        )
    }

    public func get(_ id: Int64) throws -> JournalEntry? {
        try db.query(
            "SELECT * FROM Journal WHERE playthrough_id = ? AND id = ?;",
            [playthroughID, id],
            map: Self.map
        ).first
    }

    /// All book-linked entries grouped per book in one query (newest first per
    /// book, this playthrough) — the book pane reads live caches built from this
    /// (stale-until-reselect fix, 2026-10-06); rows without a book are ungrouped.
    public func entriesByBook() throws -> [Int64: [JournalEntry]] {
        let rows = try db.query(
            """
            SELECT * FROM Journal
            WHERE playthrough_id = ? AND book_id IS NOT NULL
            ORDER BY logged_at DESC, id DESC;
            """,
            [playthroughID],
            map: Self.map
        )
        var grouped: [Int64: [JournalEntry]] = [:]
        for entry in rows {
            guard let bookID = entry.bookID else { continue }
            grouped[bookID, default: []].append(entry)
        }
        return grouped
    }

    /// Entries linked to a given entity, newest first.
    public func entries(bookID: Int64? = nil, memoryID: Int64? = nil,
                        skillID: Int64? = nil, limit: Int = 100) throws -> [JournalEntry] {
        var clauses = ["playthrough_id = ?"]
        var binds: [SQLiteBindable] = [playthroughID]
        if let bookID { clauses.append("book_id = ?"); binds.append(bookID) }
        if let memoryID { clauses.append("memory_id = ?"); binds.append(memoryID) }
        if let skillID { clauses.append("skill_id = ?"); binds.append(skillID) }
        return try db.query(
            "SELECT * FROM Journal WHERE " + clauses.joined(separator: " AND ")
                + " ORDER BY logged_at DESC, id DESC LIMIT ?;",
            binds + [limit],
            map: Self.map
        )
    }

    @discardableResult
    public func insert(_ draft: JournalDraft) throws -> JournalEntry {
        try db.execute(
            """
            INSERT INTO Journal (game_day, entry, book_id, memory_id, skill_id, playthrough_id)
            VALUES (?, ?, ?, ?, ?, ?);
            """,
            [draft.gameDay, draft.entry, draft.bookID, draft.memoryID, draft.skillID, playthroughID]
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
        try db.execute("DELETE FROM Journal WHERE playthrough_id = ? AND id = ?;",
                       [playthroughID, id])
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