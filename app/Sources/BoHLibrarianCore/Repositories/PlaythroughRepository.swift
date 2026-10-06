import Foundation

/// Playthrough lifecycle: one row per saved game, plus the Meta table holding
/// which playthrough is currently loaded. Not itself scoped (docs/DATABASE.md).
public final class PlaythroughRepository {
    private let db: SQLiteDatabase
    private static let activeKey = "active_playthrough"

    public init(db: SQLiteDatabase) {
        self.db = db
    }

    // MARK: Playthroughs

    public func all() throws -> [Playthrough] {
        try db.query("SELECT * FROM Playthroughs ORDER BY id;", map: Self.map)
    }

    public func get(_ id: Int64) throws -> Playthrough? {
        try db.query("SELECT * FROM Playthroughs WHERE id = ?;", [id], map: Self.map).first
    }

    @discardableResult
    public func insert(name: String, notes: String? = nil) throws -> Playthrough {
        try db.execute("INSERT INTO Playthroughs (name, notes) VALUES (?, ?);", [name, notes])
        return try get(db.lastInsertRowID)!
    }

    public func update(_ playthrough: Playthrough) throws {
        try db.execute(
            "UPDATE Playthroughs SET name = ?, notes = ? WHERE id = ?;",
            [playthrough.name, playthrough.notes, playthrough.id]
        )
    }

    /// Deletes the playthrough and (via ON DELETE CASCADE) every book, memory,
    /// skill and journal row scoped to it. The caller guards: never the active
    /// one, never the last one, and always confirmed by the user.
    public func delete(_ id: Int64) throws {
        try db.execute("DELETE FROM Playthroughs WHERE id = ?;", [id])
    }

    // MARK: Active playthrough

    public func activeID() throws -> Int64? {
        try db.query("SELECT value FROM Meta WHERE key = ?;", [Self.activeKey]) {
            $0.string("value").flatMap { Int64($0) }
        }.first ?? nil
    }

    public func setActiveID(_ id: Int64) throws {
        try db.execute(
            "INSERT INTO Meta (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value;",
            [Self.activeKey, String(id)]
        )
    }

    /// The loaded playthrough; the first one if Meta is missing or stale.
    public func active() throws -> Playthrough? {
        if let id = try activeID(), let playthrough = try get(id) {
            return playthrough
        }
        return try all().first
    }

    static func map(_ row: Row) throws -> Playthrough {
        Playthrough(id: try row.requireInt64("id"),
                    name: try row.requireString("name"),
                    createdAt: try row.requireString("created_at"),
                    notes: row.string("notes"))
    }
}