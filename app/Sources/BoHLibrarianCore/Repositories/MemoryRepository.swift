import Foundation

public final class MemoryRepository {
    private let db: SQLiteDatabase

    public init(db: SQLiteDatabase) { self.db = db }

    // MARK: CRUD

    public func all() throws -> [Memory] {
        let memories = try db.query("SELECT * FROM Memories ORDER BY name;", map: Self.map)
        return try attachAspects(to: memories)
    }

    public func get(_ id: Int64) throws -> Memory? {
        guard var memory = try db.query("SELECT * FROM Memories WHERE id = ?;", [id], map: Self.map).first else {
            return nil
        }
        memory.aspects = try aspects(for: id)
        return memory
    }

    @discardableResult
    public func insert(_ draft: MemoryDraft) throws -> Memory {
        let id = try insertRow(draft)
        try setAspects(id, draft.aspects)
        return try get(id)!
    }

    public func update(_ memory: Memory) throws {
        try db.transaction {
            try db.execute(
                """
                UPDATE Memories
                SET name = ?, kind = ?, persistent = ?, notes = ?, updated_at = datetime('now')
                WHERE id = ?;
                """,
                [memory.name, memory.kind.rawValue, memory.persistent, memory.notes, memory.id]
            )
            try setAspects(memory.id, memory.aspects.map { AspectDraft(principleID: $0.principleID, level: $0.level) })
        }
    }

    public func delete(_ id: Int64) throws {
        try db.execute("DELETE FROM Memories WHERE id = ?;", [id])
    }

    private func insertRow(_ draft: MemoryDraft) throws -> Int64 {
        try db.execute(
            "INSERT INTO Memories (name, kind, persistent, notes) VALUES (?, ?, ?, ?);",
            [draft.name, draft.kind.rawValue, draft.persistent, draft.notes]
        )
        return db.lastInsertRowID
    }

    // MARK: Aspects

    public func aspects(for memoryID: Int64) throws -> [Aspect] {
        try db.query(
            """
            SELECT ma.principle_id, p.name AS principle_name, ma.level
            FROM MemoryAspects ma
            JOIN Principles p ON p.id = ma.principle_id
            WHERE ma.memory_id = ?
            ORDER BY p.sort_order;
            """,
            [memoryID],
            map: Self.mapAspect
        )
    }

    /// Replace the memory's aspects entirely.
    public func setAspects(_ memoryID: Int64, _ aspects: [AspectDraft]) throws {
        try db.transaction {
            try db.execute("DELETE FROM MemoryAspects WHERE memory_id = ?;", [memoryID])
            for aspect in aspects {
                try db.execute(
                    "INSERT INTO MemoryAspects (memory_id, principle_id, level) VALUES (?, ?, ?);",
                    [memoryID, aspect.principleID, aspect.level]
                )
            }
        }
    }

    // MARK: Sources

    public func sources(for memoryID: Int64) throws -> [MemorySource] {
        try db.query(
            "SELECT kind, detail FROM MemorySources WHERE memory_id = ? ORDER BY kind, detail;",
            [memoryID]
        ) {
            MemorySource(kind: try $0.requireString("kind"), detail: $0.string("detail"))
        }
    }

    public func addSource(_ memoryID: Int64, kind: String, detail: String? = nil) throws {
        try db.execute(
            "INSERT OR IGNORE INTO MemorySources (memory_id, kind, detail) VALUES (?, ?, ?);",
            [memoryID, kind, detail]
        )
    }

    public func removeSource(_ memoryID: Int64, kind: String, detail: String? = nil) throws {
        try db.execute(
            "DELETE FROM MemorySources WHERE memory_id = ? AND kind = ? AND detail IS ?;",
            [memoryID, kind, detail]
        )
    }

    // MARK: Reading Helper (canonical query 1 — docs/DATABASE.md)

    /// Memories whose level in `principleID` is at least `minLevel`, best first.
    public func candidates(principleID: Int64, minLevel: Int) throws -> [MemoryCandidate] {
        try db.query(
            """
            SELECT m.id, m.name, m.kind, m.persistent, ma.level
            FROM Memories m
            JOIN MemoryAspects ma ON ma.memory_id = m.id
            WHERE ma.principle_id = ? AND ma.level >= ?
            ORDER BY ma.level DESC, m.name;
            """,
            [principleID, minLevel]
        ) {
            MemoryCandidate(id: try $0.requireInt64("id"),
                            name: try $0.requireString("name"),
                            kind: MemoryKind(rawValue: try $0.requireString("kind")) ?? .memory,
                            persistent: $0.bool("persistent"),
                            level: try $0.requireInt("level"))
        }
    }

    /// Books that yield this memory when read.
    public func booksYielding(_ memoryID: Int64) throws -> [BookRef] {
        try db.query(
            "SELECT id, title FROM Books WHERE yielded_memory_id = ? ORDER BY title;",
            [memoryID]
        ) {
            BookRef(id: try $0.requireInt64("id"), title: try $0.requireString("title"))
        }
    }

    // MARK: Mapping helpers

    private func attachAspects(to memories: [Memory]) throws -> [Memory] {
        guard !memories.isEmpty else { return memories }
        var byMemory: [Int64: [Aspect]] = [:]
        let rows = try db.query(
            """
            SELECT ma.memory_id, ma.principle_id, p.name AS principle_name, ma.level
            FROM MemoryAspects ma
            JOIN Principles p ON p.id = ma.principle_id
            ORDER BY p.sort_order;
            """
        ) { row in
            (memoryID: try row.requireInt64("memory_id"),
             aspect: try Self.mapAspect(row))
        }
        for row in rows {
            byMemory[row.memoryID, default: []].append(row.aspect)
        }
        return memories.map { memory in
            var updated = memory
            updated.aspects = byMemory[memory.id] ?? []
            return updated
        }
    }

    private static func mapAspect(_ row: Row) throws -> Aspect {
        Aspect(principleID: try row.requireInt64("principle_id"),
               principleName: try row.requireString("principle_name"),
               level: try row.requireInt("level"))
    }

    static func map(_ row: Row) throws -> Memory {
        Memory(id: try row.requireInt64("id"),
               name: try row.requireString("name"),
               kind: MemoryKind(rawValue: try row.requireString("kind")) ?? .memory,
               persistent: row.bool("persistent"),
               notes: row.string("notes"))
    }
}